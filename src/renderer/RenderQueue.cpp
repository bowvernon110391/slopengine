#include "renderer/RenderQueue.h"

#include <cstring>

#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Mesh.h"
#include "renderer/Frustum.h"
#include "renderer/ResourceCaches.h"
#include "scene/Scene.h"

namespace {

// Material flag bit: draw in the transparent pass.
constexpr std::uint32_t MATERIAL_FLAG_TRANSPARENT = 1u << 0;

} // namespace

void RenderQueue::clear()
{
    m_frameItems.clear();
    m_opaque.clear();
    m_transparent.clear();
    m_shadowLists.clear();
}

void RenderQueue::collectItems(Scene& scene,
                               const MeshCache& meshes,
                               const MaterialCache& materials,
                               std::uint32_t layerMask)
{
    m_frameItems.clear();

    ComponentStorage<MeshRenderer>& renderers = scene.meshes();
    std::vector<MeshRenderer>&      dense     = renderers.dense();
    const std::vector<Entity>&      owners    = renderers.entities();

    m_frameItems.reserve(dense.size());

    for (std::size_t i = 0; i < dense.size(); ++i) {
        MeshRenderer& mr = dense[i];
        const Entity e = owners[i];

        if ((mr.layerMask & layerMask) == 0) continue;

        const Mesh* mesh = meshes.get(mr.mesh);
        if (!mesh) continue;

        const Transform* transform = scene.transforms().get(e);
        const glm::mat4 model = transform ? transform->worldMatrix : glm::mat4(1.0f);

        // Local-space bounds. Fall back to the mesh's own bounds so a renderer
        // that never had them filled in cannot cull or sort itself away.
        const AABB localBounds = mr.worldBounds.valid() ? mr.worldBounds
                                                        : mesh->bounds();

        // Objects that never move can reuse their world-space bounds, saving the
        // eight corner transforms every frame. Storing the matrix the cache was
        // derived from keeps this honest: if an ancestor moved, we recompute.
        const bool effectivelyStatic = (transform && transform->isStatic)
                                    || (mr.flags & kMeshFlagStatic) != 0;

        const bool cacheHit =
            effectivelyStatic
            && mr.cachedWorldBounds.valid()
            && std::memcmp(glm::value_ptr(mr.cachedWorldMatrix),
                           glm::value_ptr(model), sizeof(glm::mat4)) == 0;

        AABB worldBounds;
        if (cacheHit) {
            worldBounds = mr.cachedWorldBounds;
        } else {
            worldBounds = localBounds.transformed(model);
            if (effectivelyStatic) {
                mr.cachedWorldBounds = worldBounds;
                mr.cachedWorldMatrix = model;
            }
        }

        RenderItem item;
        item.mesh        = mesh;
        item.material    = materials.get(mr.material);
        item.model       = model;
        item.worldBounds = worldBounds;
        item.flags       = mr.flags;
        item.id          = e.id;
        item.layerMask   = mr.layerMask;

        m_frameItems.push_back(item);
    }
}

void RenderQueue::buildCameraLists(Scene& scene,
                                   const Camera& camera,
                                   const MeshCache& meshes,
                                   const MaterialCache& materials,
                                   std::uint32_t layerMask)
{
    collectItems(scene, meshes, materials, layerMask);

    m_opaque.clear();
    m_transparent.clear();
    m_visibleCount = 0;
    m_culledCount  = 0;

    Frustum frustum;
    if (m_cullingEnabled) {
        frustum.fromViewProj(camera.projectionMatrix() * camera.viewMatrix());
    }

    for (std::size_t i = 0; i < m_frameItems.size(); ++i) {
        RenderItem& item = m_frameItems[i];

        // Culling only trims this view's draw lists. The item array keeps every
        // entry, because an off-camera object can still cast a shadow into view.
        if (m_cullingEnabled && !frustum.intersects(item.worldBounds)) {
            item.visible = false;
            ++m_culledCount;
            continue;
        }
        item.visible = true;
        ++m_visibleCount;

        const std::uint32_t index = static_cast<std::uint32_t>(i);
        const bool transparent = item.material
                              && (item.material->flags & MATERIAL_FLAG_TRANSPARENT) != 0;
        if (transparent) m_transparent.push(index);
        else             m_opaque.push(index);
    }

    const glm::vec3 eye = camera.position();
    // Opaque: grouped by material, nearest first inside each group.
    m_opaque.sortByMaterial(m_frameItems, eye);
    m_transparent.sortBackToFront(m_frameItems, eye);
}

void RenderQueue::buildShadowLists(Scene& scene,
                                   const std::vector<Light>& lights,
                                   const MeshCache& meshes,
                                   const MaterialCache& materials,
                                   std::uint32_t layerMask)
{
    collectItems(scene, meshes, materials, layerMask);

    m_shadowLists.clear();
    for (const Light& light : lights) {
        if (!light.castsShadow) continue;

        DrawList list;
        for (std::size_t i = 0; i < m_frameItems.size(); ++i) {
            list.push(static_cast<std::uint32_t>(i));
        }
        // Shadow casters are drawn front-to-back from the light's point of view;
        // orders here are approximate and only affect overdraw, not correctness.
        m_shadowLists.push_back(list);
    }
}

void RenderQueue::build(Scene& scene,
                        const Camera& camera,
                        const std::vector<Light>& lights,
                        const MeshCache& meshes,
                        const MaterialCache& materials,
                        std::uint32_t layerMask)
{
    buildShadowLists(scene, lights, meshes, materials, layerMask);
    buildCameraLists(scene, camera, meshes, materials, layerMask);
}
