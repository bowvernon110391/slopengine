#pragma once

#include "renderer/RenderPass.h"

// Resolves the multisampled forward target into a single-sample texture.
class ResolvePass : public RenderPass
{
public:
    const char* name() const override { return "ResolvePass"; }
    void execute(RenderContext& ctx) override;
};
