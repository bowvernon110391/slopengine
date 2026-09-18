#include "renderer/Renderer.h"

#include <algorithm>

#include "Mesh.h"
#include "Shader.h"
#include "scene/Scene.h"

Renderer::Renderer()
    : m_shadowPass(new ShadowPass())
    , m_forwardPass(new ForwardPass())
    , m_debugAABBPass(new DebugAABBPass())
    , m_resolvePass(new ResolvePass())
    , m_finalPostPass(new FinalPostPass())
    , m_uiPass(new UIPass())
    , m_ssaoPass(new SSAOPass())
    , m_aoCompositePass(new AOCompositePass())
    , m_bloomBrightPass(new BloomBrightPass())
    , m_bloomDownsamplePass(new BloomDownsamplePass())
    , m_bloomUpsamplePass(new BloomUpsamplePass())
{
}

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(const std::string& shaderBasePath, int shadowMapSize, int msaaSamples)
{
    m_shadowMapSize = shadowMapSize;
    m_msaaSamples   = msaaSamples;

    gl::GenVertexArrays(1, &m_fullscreenVAO);

    const std::string dir = shaderBasePath + "shaders/";
    if (!m_shaders.load("shadow_depth", dir + "shadow_depth.glsl")) return false;
    if (!m_shaders.load("forward",      dir + "forward.glsl"))      return false;
    if (!m_shaders.load("fullscreen",   dir + "fullscreen.glsl"))   return false;
    if (!m_shaders.load("debug_line",   dir + "debug_line.glsl"))   return false;
    if (!m_shaders.load("depth_preview", dir + "depth_preview.glsl")) return false;

    m_depthPreviewShader = m_shaders.get("depth_preview");

    m_initialized = true;
    return true;
}

void Renderer::shutdown()
{
    if (!m_initialized && m_fullscreenVAO == 0) return;

    for (ShadowPreviewTarget& preview : m_shadowPreviews) {
        if (preview.fbo)     gl::DeleteFramebuffers(1, &preview.fbo);
        if (preview.texture) gl::DeleteTextures(1, &preview.texture);
    }
    m_shadowPreviews.clear();
    m_shadowViews.clear();
    m_frameShadowFBOs.clear();
    m_depthPreviewShader = nullptr;

    m_pool.destroyAll();
    m_shaders.clear();

    if (m_fullscreenVAO) {
        gl::DeleteVertexArrays(1, &m_fullscreenVAO);
        m_fullscreenVAO = 0;
    }
    m_initialized = false;
}

void Renderer::renderFrame(const std::vector<RenderView>& views, Scene& scene)
{
    m_pool.beginFrame(m_frameTag);

    resetFrameStats();
    m_frameGraph.setViews(views);
    m_frameGraph.render(*this, scene);

    m_pool.releaseAll(m_frameTag);
    ++m_frameTag;
}

void Renderer::setShadowDebugRange(float minDepth, float maxDepth, bool invert)
{
    m_shadowDebugMin    = minDepth;
    m_shadowDebugMax    = maxDepth;
    m_shadowDebugInvert = invert;
}

void Renderer::setFrameShadowMaps(const std::vector<FBOHandle>& fbos)
{
    m_frameShadowFBOs = fbos;
}

void Renderer::ensureShadowPreviews(std::size_t count, int size)
{
    // A size change invalidates every target: drop them all and rebuild below.
    bool sizeMismatch = false;
    for (const ShadowPreviewTarget& preview : m_shadowPreviews) {
        if (preview.size != size) { sizeMismatch = true; break; }
    }
    if (sizeMismatch) {
        for (ShadowPreviewTarget& preview : m_shadowPreviews) {
            if (preview.fbo)     gl::DeleteFramebuffers(1, &preview.fbo);
            if (preview.texture) gl::DeleteTextures(1, &preview.texture);
        }
        m_shadowPreviews.clear();
    }

    // Drop any surplus targets.
    while (m_shadowPreviews.size() > count) {
        ShadowPreviewTarget& preview = m_shadowPreviews.back();
        if (preview.fbo)     gl::DeleteFramebuffers(1, &preview.fbo);
        if (preview.texture) gl::DeleteTextures(1, &preview.texture);
        m_shadowPreviews.pop_back();
    }

    // Create the missing ones.
    while (m_shadowPreviews.size() < count) {
        ShadowPreviewTarget preview;
        preview.size = size;

        gl::GenTextures(1, &preview.texture);
        gl::BindTexture(gl::TEXTURE_2D, preview.texture);
        gl::TexImage2D(gl::TEXTURE_2D, 0, static_cast<gl::GLint>(gl::RGBA8),
                       size, size, 0, gl::RGBA, gl::UNSIGNED_BYTE, nullptr);
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER,
                          static_cast<gl::GLint>(gl::LINEAR));
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER,
                          static_cast<gl::GLint>(gl::LINEAR));
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S,
                          static_cast<gl::GLint>(gl::CLAMP_TO_EDGE));
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T,
                          static_cast<gl::GLint>(gl::CLAMP_TO_EDGE));

        gl::GenFramebuffers(1, &preview.fbo);
        gl::BindFramebuffer(gl::FRAMEBUFFER, preview.fbo);
        gl::FramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0,
                                 gl::TEXTURE_2D, preview.texture, 0);

        const gl::GLenum drawBuf = gl::COLOR_ATTACHMENT0;
        gl::DrawBuffers(1, &drawBuf);

        const gl::GLenum status = gl::CheckFramebufferStatus(gl::FRAMEBUFFER);
        if (status != gl::FRAMEBUFFER_COMPLETE) {
            SDL_Log("Renderer: shadow preview framebuffer incomplete (status 0x%X)",
                    static_cast<unsigned>(status));
        }

        gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
        m_shadowPreviews.push_back(preview);
    }
}

void Renderer::renderShadowPreviews()
{
    m_shadowViews.clear();

    if (!m_shadowDebugEnabled || !m_depthPreviewShader) return;
    if (m_frameShadowFBOs.empty()) return;

    // 2048 squared per map would be wasteful on integrated graphics, and the
    // panel does not need the full resolution.
    const int previewSize = std::min(m_shadowMapSize, 512);
    ensureShadowPreviews(m_frameShadowFBOs.size(), previewSize);

    m_depthPreviewShader->use();
    gl::Disable(gl::DEPTH_TEST);
    gl::DepthMask(0);
    gl::BindVertexArray(m_fullscreenVAO);

    for (std::size_t i = 0; i < m_frameShadowFBOs.size(); ++i) {
        const gl::GLuint depthTex = m_pool.depthTexture(m_frameShadowFBOs[i]);
        if (depthTex == 0) continue;

        ShadowPreviewTarget& preview = m_shadowPreviews[i];

        gl::BindFramebuffer(gl::FRAMEBUFFER, preview.fbo);
        gl::Viewport(0, 0, preview.size, preview.size);

        gl::ActiveTexture(gl::TEXTURE0);
        gl::BindTexture(gl::TEXTURE_2D, depthTex);
        m_depthPreviewShader->setInt("uDepth", 0);
        m_depthPreviewShader->setFloat("uMin", m_shadowDebugMin);
        m_depthPreviewShader->setFloat("uMax", m_shadowDebugMax);
        m_depthPreviewShader->setInt("uInvert", m_shadowDebugInvert ? 1 : 0);

        gl::DrawArrays(gl::TRIANGLES, 0, 3);

        ShadowMapView view;
        view.texture     = preview.texture;
        view.previewSize = preview.size;
        view.sourceSize  = m_shadowMapSize;
        m_shadowViews.push_back(view);
    }

    gl::BindVertexArray(0);
    gl::DepthMask(1);
    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
    gl::Viewport(0, 0, m_outputWidth, m_outputHeight);
}
