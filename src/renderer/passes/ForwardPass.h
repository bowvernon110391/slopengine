#pragma once

#include "renderer/RenderPass.h"

class Shader;

// Forward-shades lit geometry into the view's multisampled target, using
// one directional light (optionally shadowed) plus a small array of point
// lights.
class ForwardPass : public RenderPass
{
public:
    const char* name() const override { return "ForwardPass"; }

    bool setup(RenderContext& ctx) override;
    void execute(RenderContext& ctx) override;

    static constexpr int kMaxPointLights = 8;

private:
    Shader* m_shader = nullptr;
};
