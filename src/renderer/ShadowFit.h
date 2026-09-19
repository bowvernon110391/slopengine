#pragma once

#include <glm/glm.hpp>

#include "Camera.h"
#include "core/Types.h"
#include "scene/Components.h"

// Parameters controlling how the directional light's orthographic projection is
// fitted to the camera.
//
// The fit concentrates shadow-map texels on what the camera can actually see,
// which trades distant shadow quality for near-camera sharpness. shadowDistance
// is therefore the primary quality knob rather than a mere safety clamp: without
// it, looking at the horizon sends the far frustum corners to the camera's far
// plane and the fit collapses into a blurry mess.
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

    // Extra world units added on each side of the light-space X/Y extents.
    // Extrusion along the light axis cannot capture a caster that sits
    // laterally outside the camera frustum; padding is what recovers those.
    float lateralPadding = 0.0f;

    // Upper bound on how far the volume is extended along the light direction to
    // capture off-screen casters. Extruding widens the near/far span, which costs
    // depth precision (shadow acne), so it is capped rather than unbounded.
    float maxExtrusion = 100.0f;

    // Extend the volume toward the light so casters outside the camera frustum
    // still cast shadows into view.
    bool extrudeForCasters = true;

    // Quantise the fitted volume to the shadow map's texel grid. Without this the
    // ortho slides in sub-texel increments as the camera moves, making shadow
    // edges crawl and shimmer.
    bool texelSnap = true;
};

// Writes the 8 world-space corners of the frustum described by
// inverse(viewProj) into 'out', ordered by (z, y, x) bit pattern.
void frustumCornersWorld(const glm::mat4& inverseViewProj, glm::vec3 out[8]);

// Builds the light-space matrix for a directional light, fitted to the camera's
// (clamped) view frustum rather than the whole scene.
//
// 'sceneBounds' is only used to work out how far toward the light to extrude in
// order to capture off-screen casters.
glm::mat4 computeShadowMatrix(const Light& light,
                              const Camera& camera,
                              const AABB& sceneBounds,
                              const ShadowFitParams& params);
