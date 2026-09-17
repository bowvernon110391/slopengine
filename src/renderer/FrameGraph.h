#pragma once

#include <vector>

#include "renderer/RenderView.h"

class Renderer;
class Scene;

// Holds the frame's views and drives the pass sequence:
//   build shadow lists -> ShadowPass -> per view: camera lists -> Forward ->
//   Resolve -> (SSAO/Bloom, disabled) -> FinalPost -> UI.
class FrameGraph
{
public:
    void setViews(const std::vector<RenderView>& views) { m_views = views; }
    void clear() { m_views.clear(); }

    std::vector<RenderView>&       views()       { return m_views; }
    const std::vector<RenderView>& views() const { return m_views; }

    // Sort by RenderView::order (stable).
    void sortViews();

    void render(Renderer& renderer, Scene& scene);

private:
    std::vector<RenderView> m_views;
};
