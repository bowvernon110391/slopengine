#include "renderer/RenderPass.h"

void FullscreenPass::drawFullscreen(const RenderContext& ctx)
{
    gl::BindVertexArray(ctx.fullscreenVAO);
    gl::DrawArrays(gl::TRIANGLES, 0, 3);
    gl::BindVertexArray(0);
}
