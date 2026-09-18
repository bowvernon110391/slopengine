#pragma once

#include <cstddef>
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

    // Shadow lists are built once per frame (all shadow-casting lights). Every
    // item is included: an object outside the camera frustum can still cast a
    // shadow into view.
    void buildShadowLists(Scene& scene,
                          const std::vector<Light>& lights,
                          const MeshCache& meshes,
                          const MaterialCache& materials,
                          std::uint32_t layerMask = 0xFFFFFFFFu);

    // Camera lists are built per view, with frustum culling applied.
    void buildCameraLists(Scene& scene,
                          const Camera& camera,
                          const MeshCache& meshes,
                          const MaterialCache& materials,
                          std::uint32_t layerMask);

    // Convenience: shadow lists + camera lists in one call.
    void build(Scene& scene,
               const Camera& camera,
               const std::vector<Light>& lights,
               const MeshCache& meshes,
               const MaterialCache& materials,
               std::uint32_t layerMask);

    const std::vector<RenderItem>& frameItems() const { return m_frameItems; }
    const DrawList& opaqueList() const      { return m_opaque; }
    const DrawList& transparentList() const { return m_transparent; }
    const std::vector<DrawList>& shadowLists() const { return m_shadowLists; }

    // Stats from the most recent buildCameraLists().
    std::size_t visibleCount()  const { return m_visibleCount; }
    std::size_t culledCount()   const { return m_culledCount; }
    std::size_t drawCallCount() const { return m_opaque.size() + m_transparent.size(); }

    // Measurement switch: with culling off the view keeps every item, so the
    // rendered image is identical and only the counters change.
    void setCullingEnabled(bool enabled) { m_cullingEnabled = enabled; }
    bool cullingEnabled() const { return m_cullingEnabled; }

private:
    void collectItems(Scene& scene,
                      const MeshCache& meshes,
                      const MaterialCache& materials,
                      std::uint32_t layerMask);

    std::vector<RenderItem> m_frameItems;
    DrawList                m_opaque;
    DrawList                m_transparent;
    std::vector<DrawList>   m_shadowLists;

    std::size_t m_visibleCount   = 0;
    std::size_t m_culledCount    = 0;
    bool        m_cullingEnabled = true;
};
