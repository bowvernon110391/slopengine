#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "core/Types.h"
#include "scene/ComponentStorage.h"
#include "scene/Components.h"

// The scene: a flat entity list plus one component storage per component type.
class Scene
{
public:
    Scene();

    // --- Entities ---------------------------------------------------------
    Entity create();
    void   destroy(Entity e);
    bool   alive(Entity e) const;

    const std::vector<Entity>& entities() const { return m_entities; }

    // --- Component storages ----------------------------------------------
    ComponentStorage<Transform>&       transforms()       { return m_transforms; }
    const ComponentStorage<Transform>& transforms() const { return m_transforms; }

    ComponentStorage<MeshRenderer>&       meshes()       { return m_meshes; }
    const ComponentStorage<MeshRenderer>& meshes() const { return m_meshes; }

    ComponentStorage<LightComponent>&       lights()       { return m_lights; }
    const ComponentStorage<LightComponent>& lights() const { return m_lights; }

    ComponentStorage<CameraComponent>&       cameras()       { return m_cameras; }
    const ComponentStorage<CameraComponent>& cameras() const { return m_cameras; }

    // --- Derived data -----------------------------------------------------
    // Recompute every Transform's cached world matrix (roots first, then
    // descend into children). Frozen (static, clean) subtrees are skipped.
    void updateTransforms();

    // Mark an entity's transform and its whole subtree dirty, so the next
    // updateTransforms() rebuilds them. This is the entry point an editor
    // inspector calls after moving a static object.
    void markTransformDirty(Entity e);

    // Recompute worldBounds over every MeshRenderer.
    void recomputeBounds();

    const AABB& worldBounds() const { return m_worldBounds; }

    // Convenience: build a render-facing Light snapshot for one entity.
    bool makeLight(Entity e, Light& out) const;

    // Flat ambient irradiance tint, multiplied by each surface's albedo. The
    // neutral default reproduces the value that used to be hardcoded in
    // shaders/common/lighting.glsl.
    glm::vec3 ambient{ 0.18f };

    std::string   name;
    std::uint64_t id = 0;

private:
    void computeWorldRecursive(Entity e, const glm::mat4& parentWorld, bool parentChanged);
    void markDirtyRecursive(Entity e);

    std::vector<Entity>        m_entities;   // live entities
    std::vector<bool>          m_alive;      // indexed by entity id
    std::vector<std::uint32_t> m_freeIds;
    std::uint32_t              m_nextId = 1;

    ComponentStorage<Transform>       m_transforms;
    ComponentStorage<MeshRenderer>    m_meshes;
    ComponentStorage<LightComponent>  m_lights;
    ComponentStorage<CameraComponent> m_cameras;

    AABB m_worldBounds;

    // Bounds contributed by frozen (static and clean) transforms. Recomputed
    // only when m_boundsDirty; dynamic items are merged in every frame.
    AABB m_staticBounds;
    bool m_boundsDirty = true;

    static std::uint64_t s_nextSceneId;
};
