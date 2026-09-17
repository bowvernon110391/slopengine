#include "renderer/passes/FinalPostPass.h"

#include "Shader.h"
#include "renderer/RenderTargetPool.h"
#include "renderer/ShaderCache.h"

bool FinalPostPass::setup(RenderContext& ctx)
{
    m_shader = ctx.shaders ? ctx.shaders->get("fullscreen") : nullptr;
    return m_shader != nullptr;
}

void FinalPostPass::execute(RenderContext& ctx)
{
    if (!m_shader || !ctx.pool || !ctx.view) return;
    if (!ctx.pool->valid(ctx.fboResolved)) return;

    const gl::GLuint targetFbo = (ctx.pool->valid(ctx.view->target))
                               ? ctx.pool->fbo(ctx.view->target)
                               : 0;
    gl::BindFramebuffer(gl::FRAMEBUFFER, targetFbo);
    gl::Viewport(ctx.viewport.x, ctx.viewport.y, ctx.viewport.width, ctx.viewport.height);

    gl::Disable(gl::DEPTH_TEST);
    gl::Disable(gl::CULL_FACE);

    m_shader->use();
    gl::ActiveTexture(gl::TEXTURE0);
    gl::BindTexture(gl::TEXTURE_2D, ctx.pool->colorTexture(ctx.fboResolved));
    m_shader->setInt("uSource", 0);

    drawFullscreen(ctx);

    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}
