#include "renderer/ShadowFit.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace {

// Picks an up vector that is never parallel to the light direction.
glm::vec3 lightUp(const glm::vec3& dir)
{
    return (std::fabs(dir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                      : glm::vec3(0.0f, 1.0f, 0.0f);
}

// Largest distance from 'center' to any of the given points: the radius of the
// bounding sphere of 'points' about 'center'.
float maxDistanceFrom(const glm::vec3& center, const glm::vec3* points, int count)
{
    float best = 0.0f;
    for (int i = 0; i < count; ++i) {
        best = std::max(best, glm::length(points[i] - center));
    }
    return best;
}

} // namespace

void frustumCornersWorld(const glm::mat4& inverseViewProj, glm::vec3 out[8])
{
    int i = 0;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                const glm::vec4 ndc(static_cast<float>(x) * 2.0f - 1.0f,
                                    static_cast<float>(y) * 2.0f - 1.0f,
                                    static_cast<float>(z) * 2.0f - 1.0f,
                                    1.0f);
                glm::vec4 p = inverseViewProj * ndc;
                // Guard against a degenerate inverse producing w == 0.
                if (std::fabs(p.w) < 1e-6f) p.w = 1.0f;
                out[i++] = glm::vec3(p) / p.w;
            }
        }
    }
}

glm::mat4 computeShadowMatrix(const Light& light,
                              const Camera& camera,
                              const ShadowFitParams& params)
{
    glm::vec3 dir = light.direction;
    if (glm::dot(dir, dir) < 1e-6f) dir = glm::vec3(0.0f, -1.0f, 0.0f);
    dir = glm::normalize(dir);

    // --- Camera frustum corners, clamped to the shadow distance -----------
    // The clamp is what keeps a horizon view from ballooning the fit.
    const float zNear  = camera.zNear();
    const float zFar   = std::max(params.shadowDistance, zNear + 0.01f);
    const float aspect = (camera.aspect() > 0.0f) ? camera.aspect() : 1.0f;

    const glm::mat4 refProj = glm::perspective(camera.fovRadians(), aspect, zNear, zFar);
    const glm::mat4 invViewProj = glm::inverse(refProj * camera.viewMatrix());

    glm::vec3 corners[8];
    frustumCornersWorld(invViewProj, corners);

    // --- Bounding sphere of the frustum slice -----------------------------
    // The slice is symmetric about its centre, so the centroid of its eight
    // corners is the centre of the bounding sphere and the distance to the
    // farthest corner is its radius. That radius depends only on
    // fov/aspect/near/far -- not on where the camera is or which way it faces:
    //   r = sqrt(((zFar - zNear) / 2)^2 + halfWidth^2 + halfHeight^2)
    //
    // A constant radius gives a constant light-space span, and therefore a
    // constant texel size, which is the precondition texel snapping needs in
    // order to hold the grid still. Fitting the slice tightly instead makes the
    // span vary with camera orientation, so a snap onto that span re-quantises
    // onto a fresh grid every frame and shadow edges crawl.
    glm::vec3 center(0.0f);
    for (const glm::vec3& c : corners) center += c;
    center /= 8.0f;

    float radius = maxDistanceFrom(center, corners, 8);
    if (!(radius > 0.0f)) radius = 1.0f;   // degenerate slice (zero fov, etc.)

    // --- Depth range ------------------------------------------------------
    // Extrusion only ever extends the range on the lightward side. It is a
    // constant rather than something derived from the scene bounds: deriving it
    // made near/far depend on the camera's position, so the range shifted every
    // frame and stored depths slid against a fixed bias, which read as acne
    // flicker while moving.
    const float extrusion = params.extrudeForCasters
                          ? std::max(params.maxExtrusion, 0.0f)
                          : 0.0f;
    const float halfDepth   = radius + extrusion;
    const float eyeDistance = halfDepth + 1.0f;

    // --- Rotation-only light basis ----------------------------------------
    // The grid axes must not rotate with the camera, so the basis is built about
    // the origin and the sphere's centre is snapped within it. Snapping inside
    // the final translated light view would let the axes' origin drift.
    const glm::mat4 lightRot = glm::lookAt(glm::vec3(0.0f), dir, lightUp(dir));

    glm::vec3 lsCenter = glm::vec3(lightRot * glm::vec4(center, 1.0f));

    // --- Texel snapping ---------------------------------------------------
    // A square box gives square texels, so the snap is isotropic and the shader's
    // Poisson disc is not stretched. Snapping the centre (rather than a corner)
    // keeps the box centred on the sphere.
    if (params.texelSnap && params.mapSize > 0) {
        const float texel = (2.0f * radius) / static_cast<float>(params.mapSize);
        if (texel > 0.0f) {
            lsCenter.x = std::floor(lsCenter.x / texel) * texel;
            lsCenter.y = std::floor(lsCenter.y / texel) * texel;
        }
    }

    const glm::vec3 snappedCenter =
        glm::vec3(glm::inverse(lightRot) * glm::vec4(lsCenter, 1.0f));

    // Looking along the light direction, so moving a point along that direction
    // changes only its light-space Z: extrusion costs no lateral texel density.
    const glm::mat4 lightView =
        glm::lookAt(snappedCenter - dir * eyeDistance, snappedCenter, lightUp(dir));

    // The box spans [-radius, radius] laterally and [-halfDepth, halfDepth] along
    // the light axis about the centre; light space looks down -Z, so the eye sits
    // eyeDistance in front of it. halfDepth >= radius, so the depth span cannot
    // collapse.
    const glm::mat4 proj = glm::ortho(-radius, radius,
                                      -radius, radius,
                                      1.0f, eyeDistance + halfDepth);
    return proj * lightView;
}
