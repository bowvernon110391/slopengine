#pragma once

#include "renderer/RenderPass.h"

// Draws the ImGui frame (built by the application) over the rendered scene.
class UIPass : public RenderPass
{
public:
    const char* name() const override { return "UIPass"; }
    void execute(RenderContext& ctx) override;
};
