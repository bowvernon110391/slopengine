#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Camera.h"
#include "core/Types.h"

// ---------------------------------------------------------------------------
// Transform: local TRS + cached world matrix, with a parent/child hierarchy.
// ---------------------------------------------------------------------------
struct Transform
{
    glm::vec3 position{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f };

    glm::mat4 worldMatrix{ 1.0f };
    bool      dirty = true;

    // When set, this node and its whole subtree skip world-matrix recomputation
    // until something calls Scene::markTransformDirty(). A static subtree must
    // not contain dynamic children.
    bool isStatic = false;

    Entity              parent{};
    std::vector<Entity> children;

    glm::mat4 localMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), position)
             * glm::mat4_cast(rotation)
             * glm::scale(glm::mat4(1.0f), scale);
    }
};

// ---------------------------------------------------------------------------
// MeshRenderer flags.
// ---------------------------------------------------------------------------
// Assert that this object never moves, so its world-space AABB can be cached
// even when it sits under a dynamic hierarchy. Invalidated by
// Scene::markTransformDirty().
constexpr std::uint32_t kMeshFlagStatic = 1u << 0;

// ---------------------------------------------------------------------------
// MeshRenderer: which mesh + material to draw, visibility layer, bounds.
// 'worldBounds' holds the mesh's local-space bounds (from Mesh::bounds()); the
// renderer transforms it by the world matrix when building draw lists.
// ---------------------------------------------------------------------------
struct MeshRenderer
{
    MeshHandle    mesh;
    MaterialHandle material;
    std::uint32_t flags     = 0;
    std::uint32_t layerMask = 1u;
    AABB          worldBounds;

    // World-space bounds cached across frames while the object is effectively
    // static. 'cachedWorldMatrix' records the matrix they were derived from, so
    // the cache is self-validating even if an ancestor moves. An invalid
    // cachedWorldBounds means "not computed yet".
    AABB      cachedWorldBounds;
    glm::mat4 cachedWorldMatrix{ 0.0f };
};

// ---------------------------------------------------------------------------
// Lights
// ---------------------------------------------------------------------------
enum class LightType : std::uint32_t
{
    None = 0,
    Directional,
    Point,
    Spot,
};

struct LightComponent
{
    LightType     type        = LightType::Point;
    glm::vec3     color{ 1.0f };
    float         intensity   = 1.0f;
    float         range       = 10.0f;
    float         innerAngle  = 0.0f;  // radians (spot)
    float         outerAngle  = 0.0f;  // radians (spot)
    bool          castsShadow = false;
};

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
struct CameraComponent
{
    Camera camera;
    bool   primary = true;
};

// ---------------------------------------------------------------------------
// Material
// ---------------------------------------------------------------------------
struct Material
{
    glm::vec3     albedo{ 1.0f };
    float         specPower    = 32.0f;
    float         specStrength = 0.35f;
    std::uint32_t flags        = 0;
};

// ---------------------------------------------------------------------------
// Light: render-facing world-space snapshot derived from a LightComponent and
// its Transform. This is what RenderView/RenderContext carry.
// ---------------------------------------------------------------------------
struct Light
{
    LightType     type        = LightType::Point;
    glm::vec3     position{ 0.0f };
    glm::vec3     direction{ 0.0f, -1.0f, 0.0f };  // travel direction
    glm::vec3     color{ 1.0f };
    float         intensity   = 1.0f;
    float         range       = 10.0f;
    float         innerAngle  = 0.0f;
    float         outerAngle  = 0.0f;
    bool          castsShadow = false;
};
