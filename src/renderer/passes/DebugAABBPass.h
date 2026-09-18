#pragma once

#include "renderer/DebugDraw.h"
#include "renderer/RenderPass.h"

class Shader;

// Draws a wireframe box around every render item's world-space AABB, colouring
// culled items differently so frustum culling can be verified by eye.
//
// This pass depth-tests against the MSAA target's depth buffer, so it must run
// after the forward pass but before the resolve (the resolved target has no
// depth attachment, and the final post pass turns depth testing off).
class DebugAABBPass : public RenderPass
{
public:
    const char* name() const override { return "DebugAABBPass"; }

    bool setup(RenderContext& ctx) override;
    void execute(RenderContext& ctx) override;

private:
    Shader*   m_shader = nullptr;
    DebugDraw m_lines;
};
