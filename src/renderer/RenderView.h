#pragma once

#include <cstdint>
#include <vector>

#include "Camera.h"
#include "core/Types.h"
#include "renderer/RenderQueue.h"
#include "renderer/ViewResources.h"
#include "scene/Components.h"

class Scene;

struct Viewport
{
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;

    Viewport() = default;
    Viewport(int x_, int y_, int w_, int h_) : x(x_), y(y_), width(w_), height(h_) {}
};

// PassMask bits select which passes a view runs.
enum PassMask : std::uint32_t
{
    Pass_Shadow      = 1u << 0,
    Pass_Forward     = 1u << 1,
    Pass_Resolve     = 1u << 2,
    Pass_SSAO        = 1u << 3,
    Pass_AOComposite = 1u << 4,
    Pass_Bloom       = 1u << 5,
    Pass_FinalPost   = 1u << 6,
    Pass_UI          = 1u << 7,
    Pass_All         = 0xFFFFFFFFu,
    Pass_Default     = Pass_Shadow | Pass_Forward | Pass_Resolve | Pass_FinalPost | Pass_UI,
};

// A single view (camera + target + passes) that the FrameGraph renders.
struct RenderView
{
    Scene*             scene = nullptr;
    Camera             camera;
    std::vector<Light> lights;
    Viewport           viewport;
    FBOHandle          target;                     // invalid => default framebuffer
    std::uint32_t      passes    = Pass_Default;
    std::uint32_t      layerMask = 1u;
    int                order     = 0;
    std::uint32_t      flags     = 0;

    RenderQueue   queue;
    ViewResources resources;
};
