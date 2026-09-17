#pragma once

#include "renderer/RenderPass.h"

// ---------------------------------------------------------------------------
// SSAO + Bloom passes.
//
// These classes exist so the FrameGraph and RenderContext are wired exactly as
// in the design diagram, but they are intentionally DISABLED: their targets
// (fboSSAO / fboSSAOBlur / fboBloom) are only acquired when the corresponding
// PassMask bit is set, which the default view mask does not do. execute() is a
// no-op until a real implementation is added.
//
// To enable later: acquire the targets in FrameGraph, set the view's PassMask
// bits, call setEnabled(true), and implement execute().
// ---------------------------------------------------------------------------

class SSAOPass : public FullscreenPass
{
public:
    const char* name() const override { return "SSAOPass"; }
    void execute(RenderContext& ctx) override { (void)ctx; }
};

class SSAOBlurPass : public FullscreenPass
{
public:
    const char* name() const override { return "SSAOBlurPass"; }
    void execute(RenderContext& ctx) override { (void)ctx; }
};

class AOCompositePass : public FullscreenPass
{
public:
    const char* name() const override { return "AOCompositePass"; }
    void execute(RenderContext& ctx) override { (void)ctx; }
};

class BloomBrightPass : public FullscreenPass
{
public:
    const char* name() const override { return "BloomBrightPass"; }
    void execute(RenderContext& ctx) override { (void)ctx; }
};

class BloomDownsamplePass : public FullscreenPass
{
public:
    const char* name() const override { return "BloomDownsamplePass"; }
    void execute(RenderContext& ctx) override { (void)ctx; }
};

class BloomUpsamplePass : public FullscreenPass
{
public:
    const char* name() const override { return "BloomUpsamplePass"; }
    void execute(RenderContext& ctx) override { (void)ctx; }
};
