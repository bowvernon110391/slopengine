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

    // Depth offset fed to glPolygonOffset() while filling the shadow maps,
    // i.e. offset = factor * slope + units, applied in window-space depth.
    //
    // The pass culls front faces, so what it stores is the far surface of each
    // caster. That pushes the stored depth away from the light by roughly the
    // caster's thickness along the light, which is precisely what leaves the
    // faint lit gap at a contact point. A negative offset pulls the stored depth
    // back toward the light and closes that gap; a positive one would widen it.
    //
    // 'units' is in depth-buffer LSBs, so its useful magnitude scales with the
    // fitted depth range rather than being a fixed constant: 1 LSB is about
    // (far - near) / 2^24 world units here. Treat it as something to sweep and
    // observe, not to derive. 'factor' scales with the polygon's depth slope, so
    // a negative factor bites hardest at grazing angles -- where acne is worst --
    // which is why it starts at 0 and 'units' does the work.
    float polygonOffsetFactor() const { return m_offsetFactor; }
    float polygonOffsetUnits()  const { return m_offsetUnits; }
    void  setPolygonOffset(float factor, float units)
    {
        m_offsetFactor = factor;
        m_offsetUnits  = units;
    }

private:
    Shader* m_shader = nullptr;

    float m_offsetFactor = 0.0f;
    float m_offsetUnits  = -512.0f;
};
