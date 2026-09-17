#pragma once

#include <cstdint>
#include <vector>

#include "renderer/DrawList.h"
#include "renderer/RenderItem.h"
#include "scene/Components.h"

class Camera;
class MaterialCache;
class MeshCache;
class Scene;

// Builds the per-frame draw data for a view from a Scene. 'frameItems' is the
// master array; DrawLists hold indices into it.
class RenderQueue
{
public:
    void clear();

    // Shadow lists are built once per frame (all shadow-casting lights).
    void buildShadowLists(const Scene& scene,
                          const std::vector<Light>& lights,
                          const MeshCache& meshes,
                          const MaterialCache& materials,
                          std::uint32_t layerMask = 0xFFFFFFFFu);

    // Camera lists are built per view.
    void buildCameraLists(const Scene& scene,
                          const Camera& camera,
                          const MeshCache& meshes,
                          const MaterialCache& materials,
                          std::uint32_t layerMask);

    // Convenience: shadow lists + camera lists in one call.
    void build(const Scene& scene,
               const Camera& camera,
               const std::vector<Light>& lights,
               const MeshCache& meshes,
               const MaterialCache& materials,
               std::uint32_t layerMask);

    const std::vector<RenderItem>& frameItems() const { return m_frameItems; }
    const DrawList& opaqueList() const      { return m_opaque; }
    const DrawList& transparentList() const { return m_transparent; }
    const std::vector<DrawList>& shadowLists() const { return m_shadowLists; }

private:
    void collectItems(const Scene& scene,
                      const MeshCache& meshes,
                      const MaterialCache& materials,
                      std::uint32_t layerMask);

    std::vector<RenderItem> m_frameItems;
    DrawList                m_opaque;
    DrawList                m_transparent;
    std::vector<DrawList>   m_shadowLists;
};
