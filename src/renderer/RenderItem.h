#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Types.h"

class Mesh;
struct Material;

// One drawable built per-frame from a Scene's MeshRenderer + Transform.
// RenderItems live in RenderQueue::frameItems; DrawCommands index into them.
struct RenderItem
{
    const Mesh*     mesh     = nullptr;
    const Material* material = nullptr;

    glm::mat4   model{ 1.0f };
    AABB        worldBounds;
    std::uint32_t flags     = 0;
    std::uint32_t id        = 0;   // entity id
    std::uint32_t layerMask = 1u;

    // Whether the view's frustum kept this item. Culled items stay in the array
    // so the shadow pass can still use them as casters, and so the AABB debug
    // overlay can show what was rejected.
    bool          visible    = true;
};
