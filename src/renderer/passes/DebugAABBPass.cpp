#include "renderer/passes/DebugAABBPass.h"

#include "Shader.h"
#include "renderer/RenderQueue.h"
#include "renderer/RenderTargetPool.h"
#include "renderer/ShaderCache.h"

namespace {

const glm::vec3 kVisibleColor(0.20f, 1.00f, 0.35f);   // green: drawn this frame
const glm::vec3 kCulledColor (1.00f, 0.30f, 0.30f);   // red: culled by the frustum

} // namespace

bool DebugAABBPass::setup(RenderContext& ctx)
{
    m_shader = ctx.shaders ? ctx.shaders->get("debug_line") : nullptr;
    return m_shader != nullptr;
}

void DebugAABBPass::execute(RenderContext& ctx)
{
    if (!m_shader || !ctx.pool || !ctx.queue || !ctx.view) return;
    if (!ctx.pool->valid(ctx.fboMainMS)) return;

    gl::BindFramebuffer(gl::FRAMEBUFFER, ctx.pool->fbo(ctx.fboMainMS));
    gl::Viewport(ctx.viewport.x, ctx.viewport.y, ctx.viewport.width, ctx.viewport.height);

    // Depth-test against the scene so boxes are correctly occluded, but never
    // write depth: this is an overlay and must not disturb later passes.
    gl::Enable(gl::DEPTH_TEST);
    gl::DepthFunc(gl::LEQUAL);
    gl::DepthMask(0);
    gl::Disable(gl::CULL_FACE);

    m_lines.begin();
    for (const RenderItem& item : ctx.queue->frameItems()) {
        m_lines.addAABB(item.worldBounds, item.visible ? kVisibleColor : kCulledColor);
    }
    m_lines.flush(*m_shader,
                  ctx.view->camera.viewMatrix(),
                  ctx.view->camera.projectionMatrix());

    gl::DepthMask(1);
    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}
