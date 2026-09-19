#pragma once

#include <glm/glm.hpp>

#include "Camera.h"
#include "scene/Components.h"

// Parameters controlling how the directional light's orthographic projection is
// fitted to the camera.
//
// The fit uses the bounding sphere of the camera's frustum slice, clamped to
// shadowDistance. A sphere's light-space extent does not change as the camera
// rotates, so the shadow map's texel size stays fixed and texel snapping can
// hold the grid still; fitting the slice tightly instead makes the span vary
// with orientation, and shadow edges then crawl. The price is that a sphere
// circumscribes the slice, so it covers somewhat more area than a tight fit.
//
// shadowDistance is therefore the primary quality knob rather than a mere safety
// clamp: without it, looking at the horizon sends the far slice corner to the
// camera's far plane and the fixed-radius sphere spreads those texels very
// thinly.
struct ShadowFitParams
{
    // Shadow map resolution, used for texel snapping. Kept in sync with
    // Renderer::shadowMapSize() by the caller.
    int   mapSize = 2048;

    // How far from the camera shadows are computed, in world units. Clamped
    // internally to the camera's far plane. Smaller => sharper near shadows.
    float shadowDistance = 40.0f;

    // Fraction of shadowDistance over which shadows fade out, so the range
    // boundary is a soft gradient rather than a hard line on the ground.
    float fadeFraction = 0.15f;

    // How far the volume is extended along the light direction to capture
    // casters outside the camera frustum. A constant, independent of the camera,
    // so that the depth range does not shift as the camera moves; extruding
    // widens that range, which costs depth precision (shadow acne).
    float maxExtrusion = 100.0f;

    // Extend the volume toward the light so casters outside the camera frustum
    // still cast shadows into view.
    bool extrudeForCasters = true;

    // Quantise the sphere's centre to the shadow map's texel grid. The fit's
    // texel size is constant, so this holds the grid fixed relative to the world
    // and stops shadow edges from shimmering as the camera moves.
    bool texelSnap = true;
};

// Writes the 8 world-space corners of the frustum described by
// inverse(viewProj) into 'out', ordered by (z, y, x) bit pattern.
void frustumCornersWorld(const glm::mat4& inverseViewProj, glm::vec3 out[8]);

// Builds the light-space matrix for a directional light, fitted to the bounding
// sphere of the camera's (clamped) frustum slice rather than the whole scene.
glm::mat4 computeShadowMatrix(const Light& light,
                              const Camera& camera,
                              const ShadowFitParams& params);
