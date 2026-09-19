#include "renderer/passes/ShadowPass.h"

#include "Mesh.h"
#include "Shader.h"
#include "renderer/RenderQueue.h"
#include "renderer/RenderTargetPool.h"
#include "renderer/ShaderCache.h"

bool ShadowPass::setup(RenderContext& ctx)
{
    m_shader = ctx.shaders ? ctx.shaders->get("shadow_depth") : nullptr;
    return m_shader != nullptr;
}

void ShadowPass::execute(RenderContext& ctx)
{
    if (!m_shader || !ctx.pool || !ctx.shadowItems || !ctx.shadowLists) return;

    std::size_t index = 0;
    for (const FBOHandle& fboHandle : ctx.fboShadow) {
        if (index >= ctx.shadowLists->size()) break;
        if (!ctx.pool->valid(fboHandle)) { ++index; continue; }

        const int w = ctx.pool->width(fboHandle);
        const int h = ctx.pool->height(fboHandle);

        gl::BindFramebuffer(gl::FRAMEBUFFER, ctx.pool->fbo(fboHandle));
        gl::Viewport(0, 0, w, h);

        gl::Enable(gl::DEPTH_TEST);
        gl::DepthFunc(gl::LEQUAL);
        gl::DepthMask(1);
        gl::Enable(gl::CULL_FACE);
        // Storing the far surface is what keeps acne away, at the cost of a slight
        // detachment at a contact point: the depth recorded is the caster's far
        // side rather than the surface. glPolygonOffset() was tried to pull that
        // back in and removed again; it trades the gap for acne, and the value is
        // driver-specific so it never settled.
        gl::CullFace(gl::FRONT);
        gl::FrontFace(gl::CCW);
        gl::Clear(gl::DEPTH_BUFFER_BIT);

        m_shader->use();

        const glm::mat4 lightMatrix = (index < ctx.lightSpaceMatrices.size())
                                    ? ctx.lightSpaceMatrices[index]
                                    : glm::mat4(1.0f);
        m_shader->setMat4("uLightSpaceMatrix", lightMatrix);

        for (const DrawCommand& cmd : (*ctx.shadowLists)[index].commands()) {
            if (cmd.itemIndex >= ctx.shadowItems->size()) continue;
            const RenderItem& item = (*ctx.shadowItems)[cmd.itemIndex];
            if (!item.mesh) continue;
            m_shader->setMat4("uModel", item.model);
            item.mesh->draw();
        }

        gl::CullFace(gl::BACK);
        ++index;
    }

    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}
