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
        // Culling the front faces stores the far surface, which is what keeps
        // acne away -- but it also pushes the stored depth away from the light
        // by the caster's thickness, causing the lit gap at a contact point that
        // the negative offset below pulls back in.
        gl::CullFace(gl::FRONT);
        gl::FrontFace(gl::CCW);
        // glPolygonOffset does not affect glClear, so this order is only for
        // readability: the offset applies to the draws below, not the clear.
        gl::Enable(gl::POLYGON_OFFSET_FILL);
        gl::PolygonOffset(m_offsetFactor, m_offsetUnits);
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

        // Must be turned off here: leaving polygon offset enabled would leak the
        // offset into the main pass's depth writes and z-fight the lit scene.
        gl::CullFace(gl::BACK);
        gl::Disable(gl::POLYGON_OFFSET_FILL);
        ++index;
    }

    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}
