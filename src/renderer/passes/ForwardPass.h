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

    // Depth-bias terms for the shadow comparison, handed to `slopeScaledBias()` in
    // shaders/common/lighting.glsl. These trade shadow acne against shadows
    // detaching from their caster, so they are worth tuning at runtime:
    //   - bias is the base depth offset;
    //   - slopeBias widens it in proportion to tan(theta) between the surface and
    //     the light, which is how the error scales;
    //   - maxBias caps it so a near edge-on surface cannot bias itself out of range
    //     and lose its shadow entirely.
    float shadowBias() const { return m_shadowBias; }
    void  setShadowBias(float v) { m_shadowBias = v; }

    float shadowSlopeBias() const { return m_shadowSlopeBias; }
    void  setShadowSlopeBias(float v) { m_shadowSlopeBias = v; }

    float shadowMaxBias() const { return m_shadowMaxBias; }
    void  setShadowMaxBias(float v) { m_shadowMaxBias = v; }

    // Restores the compile-time defaults, so a tuning session can be undone.
    void resetShadowBias()
    {
        m_shadowBias       = kDefaultShadowBias;
        m_shadowSlopeBias  = kDefaultShadowSlopeBias;
        m_shadowMaxBias    = kDefaultShadowMaxBias;
    }

    // Poisson-disc shadow filter radius, in shadow-map texels. Widening it softens
    // the penumbra without costing extra fetches, so it is the cheap quality knob.
    float pcfRadius() const { return m_pcfRadius; }
    void  setPcfRadius(float r) { m_pcfRadius = r; }

private:
    static constexpr float kDefaultShadowBias      = 0.0f;
    static constexpr float kDefaultShadowSlopeBias = 0.35f;
    static constexpr float kDefaultShadowMaxBias   = 0.02f;
    static constexpr float kDefaultPcfRadius       = 2.0f;

    Shader* m_shader = nullptr;

    float m_shadowBias      = kDefaultShadowBias;
    float m_shadowSlopeBias = kDefaultShadowSlopeBias;
    float m_shadowMaxBias   = kDefaultShadowMaxBias;
    float m_pcfRadius       = kDefaultPcfRadius;
};
