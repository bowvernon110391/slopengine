#pragma once

#include "GL.h"
#include "renderer/RenderContext.h"

// Abstract render pass. setup() runs once per invocation before execute();
// teardown() after. FullscreenPass adds a helper for drawing a full-screen
// triangle into the currently bound target.
class RenderPass
{
public:
    virtual ~RenderPass() = default;

    virtual const char* name() const = 0;

    virtual bool setup(RenderContext& ctx) { (void)ctx; return true; }
    virtual void execute(RenderContext& ctx) = 0;
    virtual void teardown(RenderContext& ctx) { (void)ctx; }

    // Used by the stubbed SSAO/Bloom passes to report whether they do work.
    bool enabled() const { return m_enabled; }
    void setEnabled(bool e) { m_enabled = e; }

protected:
    bool m_enabled = false;
};

class FullscreenPass : public RenderPass
{
protected:
    // Draws a full-screen triangle using ctx.fullscreenVAO (no VBO needed).
    static void drawFullscreen(const RenderContext& ctx);
};
