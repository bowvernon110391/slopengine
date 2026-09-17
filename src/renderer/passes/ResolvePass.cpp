#include "renderer/passes/ResolvePass.h"

#include "renderer/RenderTargetPool.h"

void ResolvePass::execute(RenderContext& ctx)
{
    if (!ctx.pool) return;
    if (!ctx.pool->valid(ctx.fboMainMS) || !ctx.pool->valid(ctx.fboResolved)) return;

    const int w = ctx.pool->width(ctx.fboMainMS);
    const int h = ctx.pool->height(ctx.fboMainMS);

    gl::BindFramebuffer(gl::READ_FRAMEBUFFER, ctx.pool->fbo(ctx.fboMainMS));
    gl::BindFramebuffer(gl::DRAW_FRAMEBUFFER, ctx.pool->fbo(ctx.fboResolved));
    gl::BlitFramebuffer(0, 0, w, h,
                        0, 0, w, h,
                        gl::COLOR_BUFFER_BIT, gl::NEAREST);
    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}
