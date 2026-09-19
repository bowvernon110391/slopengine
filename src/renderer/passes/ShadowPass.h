#pragma once

#include "renderer/RenderPass.h"

class Shader;

// Renders the depth of every shadow-casting light into its shadow map.
class ShadowPass : public RenderPass
{
public:
    const char* name() const override { return "ShadowPass"; }

    bool setup(RenderContext& ctx) override;
    void execute(RenderContext& ctx) override;

private:
    Shader* m_shader = nullptr;
};
