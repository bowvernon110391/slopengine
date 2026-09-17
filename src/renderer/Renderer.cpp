#include "renderer/Renderer.h"

#include "Mesh.h"
#include "scene/Scene.h"

Renderer::Renderer()
    : m_shadowPass(new ShadowPass())
    , m_forwardPass(new ForwardPass())
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

    m_initialized = true;
    return true;
}

void Renderer::shutdown()
{
    if (!m_initialized && m_fullscreenVAO == 0) return;

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

    m_frameGraph.setViews(views);
    m_frameGraph.render(*this, scene);

    m_pool.releaseAll(m_frameTag);
    ++m_frameTag;
}
