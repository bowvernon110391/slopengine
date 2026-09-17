#include "renderer/passes/UIPass.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "renderer/RenderTargetPool.h"

void UIPass::execute(RenderContext& ctx)
{
    if (!ctx.pool || !ctx.view) return;

    const gl::GLuint targetFbo = (ctx.pool->valid(ctx.view->target))
                               ? ctx.pool->fbo(ctx.view->target)
                               : 0;
    gl::BindFramebuffer(gl::FRAMEBUFFER, targetFbo);
    gl::Viewport(ctx.viewport.x, ctx.viewport.y, ctx.viewport.width, ctx.viewport.height);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}
