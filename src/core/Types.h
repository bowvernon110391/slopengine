#pragma once

#include <cstdint>
#include <limits>

#include <glm/glm.hpp>

// ---------------------------------------------------------------------------
// Entity
// ---------------------------------------------------------------------------
struct Entity
{
    std::uint32_t id = 0;
    bool valid() const { return id != 0; }
};

inline bool operator==(Entity a, Entity b) { return a.id == b.id; }
inline bool operator!=(Entity a, Entity b) { return a.id != b.id; }

// ---------------------------------------------------------------------------
// Resource handles
//
// Opaque indices into renderer-owned caches/tables. id == 0 is always invalid,
// so a default-constructed handle means "none".
// ---------------------------------------------------------------------------
struct MeshHandle
{
    std::uint32_t id = 0;
    bool valid() const { return id != 0; }
};

struct MaterialHandle
{
    std::uint32_t id = 0;
    bool valid() const { return id != 0; }
};

// A framebuffer handle into RenderTargetPool. The generation guards against a
// stale handle pointing at a pool slot that has since been recycled.
struct FBOHandle
{
    std::uint32_t id = 0;
    std::uint32_t generation = 0;
    bool valid() const { return id != 0; }
};

// ---------------------------------------------------------------------------
// Axis-aligned bounding box
// ---------------------------------------------------------------------------
struct AABB
{
    glm::vec3 min{  std::numeric_limits<float>::max() };
    glm::vec3 max{ -std::numeric_limits<float>::max() };

    bool valid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    void reset()
    {
        min = glm::vec3(  std::numeric_limits<float>::max());
        max = glm::vec3( -std::numeric_limits<float>::max());
    }

    void expand(const glm::vec3& p)
    {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    void merge(const AABB& other)
    {
        if (!other.valid()) return;
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    glm::vec3 center()  const { return (min + max) * 0.5f; }
    glm::vec3 extents() const { return (max - min) * 0.5f; }
    float     radius()  const { return glm::length(extents()); }

    // AABB of the eight corners after applying 'm' (a conservative fit for the
    // transformed box; tight for axis-aligned transforms).
    AABB transformed(const glm::mat4& m) const
    {
        AABB out;
        if (!valid()) return out;
        for (int i = 0; i < 8; ++i) {
            const glm::vec3 corner((i & 1) ? max.x : min.x,
                                   (i & 2) ? max.y : min.y,
                                   (i & 4) ? max.z : min.z);
            out.expand(glm::vec3(m * glm::vec4(corner, 1.0f)));
        }
        return out;
    }
};
