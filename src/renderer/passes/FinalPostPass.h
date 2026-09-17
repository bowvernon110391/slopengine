#pragma once

#include "renderer/RenderPass.h"

class Shader;

// Final post pass: draws the resolved image to the view's output target
// (or the default framebuffer). Tone mapping / colour grading would live here.
class FinalPostPass : public FullscreenPass
{
public:
    const char* name() const override { return "FinalPostPass"; }

    bool setup(RenderContext& ctx) override;
    void execute(RenderContext& ctx) override;

private:
    Shader* m_shader = nullptr;
};
