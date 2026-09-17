#include "renderer/RenderQueue.h"

#include "Camera.h"
#include "Mesh.h"
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

void RenderQueue::collectItems(const Scene& scene,
                               const MeshCache& meshes,
                               const MaterialCache& materials,
                               std::uint32_t layerMask)
{
    m_frameItems.clear();

    const ComponentStorage<MeshRenderer>& renderers = scene.meshes();
    const std::vector<MeshRenderer>& dense = renderers.dense();
    const std::vector<Entity>& owners = renderers.entities();

    m_frameItems.reserve(dense.size());

    for (std::size_t i = 0; i < dense.size(); ++i) {
        const Entity e = owners[i];
        const MeshRenderer& mr = dense[i];

        if ((mr.layerMask & layerMask) == 0) continue;

        const Mesh* mesh = meshes.get(mr.mesh);
        if (!mesh) continue;

        const Transform* transform = scene.transforms().get(e);
        const glm::mat4 model = transform ? transform->worldMatrix : glm::mat4(1.0f);

        RenderItem item;
        item.mesh        = mesh;
        item.material    = materials.get(mr.material);
        item.model       = model;
        item.worldBounds = mr.worldBounds.transformed(model);
        item.flags       = mr.flags;
        item.id          = e.id;
        item.layerMask   = mr.layerMask;

        m_frameItems.push_back(item);
    }
}

void RenderQueue::buildCameraLists(const Scene& scene,
                                   const Camera& camera,
                                   const MeshCache& meshes,
                                   const MaterialCache& materials,
                                   std::uint32_t layerMask)
{
    collectItems(scene, meshes, materials, layerMask);

    m_opaque.clear();
    m_transparent.clear();

    for (std::uint32_t i = 0; i < m_frameItems.size(); ++i) {
        const RenderItem& item = m_frameItems[i];
        const bool transparent = item.material
                              && (item.material->flags & MATERIAL_FLAG_TRANSPARENT) != 0;
        if (transparent) m_transparent.push(i);
        else             m_opaque.push(i);
    }

    const glm::vec3 eye = camera.position();
    m_opaque.sortFrontToBack(m_frameItems, eye);
    m_transparent.sortBackToFront(m_frameItems, eye);
}

void RenderQueue::buildShadowLists(const Scene& scene,
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
        for (std::uint32_t i = 0; i < m_frameItems.size(); ++i) {
            list.push(i);
        }
        // Shadow casters are drawn front-to-back from the light's point of view;
        // orders here are approximate and only affect overdraw, not correctness.
        m_shadowLists.push_back(list);
    }
}

void RenderQueue::build(const Scene& scene,
                        const Camera& camera,
                        const std::vector<Light>& lights,
                        const MeshCache& meshes,
                        const MaterialCache& materials,
                        std::uint32_t layerMask)
{
    buildShadowLists(scene, lights, meshes, materials, layerMask);
    buildCameraLists(scene, camera, meshes, materials, layerMask);
}
