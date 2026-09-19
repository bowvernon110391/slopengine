#include "renderer/ShadowFit.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

namespace {

// Picks an up vector that is never parallel to the light direction.
glm::vec3 lightUp(const glm::vec3& dir)
{
    return (std::fabs(dir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                      : glm::vec3(0.0f, 1.0f, 0.0f);
}

// Largest distance from 'center' to any of the given points. Used to place the
// light's eye far enough back that every point lands in front of it.
float maxDistanceFrom(const glm::vec3& center, const glm::vec3* points, int count)
{
    float best = 0.0f;
    for (int i = 0; i < count; ++i) {
        best = std::max(best, glm::length(points[i] - center));
    }
    return best;
}

glm::vec3 aabbCenter(const AABB& box)
{
    return box.valid() ? box.center() : glm::vec3(0.0f);
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
                              const AABB& sceneBounds,
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

    // Scene bounds corners, for working out how far the light reaches past the
    // visible region.
    glm::vec3 sceneCorners[8];
    const bool haveSceneBounds = sceneBounds.valid();
    if (haveSceneBounds) {
        for (int i = 0; i < 8; ++i) {
            sceneCorners[i] = glm::vec3((i & 1) ? sceneBounds.max.x : sceneBounds.min.x,
                                        (i & 2) ? sceneBounds.max.y : sceneBounds.min.y,
                                        (i & 4) ? sceneBounds.max.z : sceneBounds.min.z);
        }
    }

    // --- Light view -------------------------------------------------------
    // Looking along the light direction, so moving a point along that direction
    // changes only its light-space Z. Extruding later therefore costs no lateral
    // texel density.
    glm::vec3 center(0.0f);
    for (const glm::vec3& c : corners) center += c;
    center /= 8.0f;

    // Place the eye far enough back that every point is in front of it.
    float far = maxDistanceFrom(center, corners, 8);
    if (haveSceneBounds) {
        far = std::max(far, maxDistanceFrom(center, sceneCorners, 8));
    }
    const float eyeDistance = far + 1.0f;

    const glm::mat4 lightView =
        glm::lookAt(center - dir * eyeDistance, center, lightUp(dir));

    // --- Extents in light space -------------------------------------------
    const float inf = std::numeric_limits<float>::max();
    float minX = inf, maxX = -inf;
    float minY = inf, maxY = -inf;
    float cMinZ = inf, cMaxZ = -inf;

    for (const glm::vec3& c : corners) {
        const glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1.0f));
        minX = std::min(minX, ls.x);
        maxX = std::max(maxX, ls.x);
        minY = std::min(minY, ls.y);
        maxY = std::max(maxY, ls.y);
        cMinZ = std::min(cMinZ, ls.z);
        cMaxZ = std::max(cMaxZ, ls.z);
    }

    float zMax = cMaxZ;

    // --- Extrusion toward the light (off-screen casters) -------------------
    // Light-space Z grows toward the light, so a caster that can shadow the
    // visible region sits at a larger Z than the frustum does. Extending zMax
    // toward the scene's most lightward point pulls those casters into the
    // volume.
    if (params.extrudeForCasters && haveSceneBounds) {
        float sMaxZ = -inf;
        for (const glm::vec3& c : sceneCorners) {
            const glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1.0f));
            sMaxZ = std::max(sMaxZ, ls.z);
        }
        const float extrusion = std::min(std::max(sMaxZ - cMaxZ, 0.0f),
                                         std::max(params.maxExtrusion, 0.0f));
        zMax += extrusion;
    }

    const float zMin = cMinZ;

    // --- Lateral padding --------------------------------------------------
    if (params.lateralPadding > 0.0f) {
        minX -= params.lateralPadding;
        maxX += params.lateralPadding;
        minY -= params.lateralPadding;
        maxY += params.lateralPadding;
    }

    // --- Texel snapping ---------------------------------------------------
    // Keep the span, but shift the origin onto whole texels so the grid stays
    // fixed relative to the world as the camera moves.
    if (params.texelSnap && params.mapSize > 0) {
        const float spanX = maxX - minX;
        const float spanY = maxY - minY;
        const float texelX = spanX / static_cast<float>(params.mapSize);
        const float texelY = spanY / static_cast<float>(params.mapSize);
        if (texelX > 0.0f) minX = std::floor(minX / texelX) * texelX;
        if (texelY > 0.0f) minY = std::floor(minY / texelY) * texelY;
        maxX = minX + spanX;
        maxY = minY + spanY;
    }

    // Light space looks down -Z, so near/far map from -zMax / -zMin.
    // A minimal depth span would make the projection singular.
    float nearPlane = -zMax;
    float farPlane  = -zMin;
    if (farPlane - nearPlane < 0.01f) farPlane = nearPlane + 0.01f;

    const glm::mat4 proj = glm::ortho(minX, maxX, minY, maxY, nearPlane, farPlane);
    return proj * lightView;
}
