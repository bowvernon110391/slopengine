#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "core/Types.h"
#include "renderer/RenderView.h"
#include "renderer/ShadowFit.h"

class MeshCache;
class MaterialCache;
class RenderTargetPool;
class Renderer;
class ShaderCache;

// Everything a RenderPass needs for one execute() call.
struct RenderContext
{
    RenderQueue* queue = nullptr;   // the current view's queue
    RenderView*  view  = nullptr;
    class Scene* scene = nullptr;
    const std::vector<Light>* lights = nullptr;

    const MeshCache*     meshes    = nullptr;
    const MaterialCache* materials = nullptr;
    ShaderCache*         shaders   = nullptr;
    RenderTargetPool*    pool      = nullptr;

    Viewport viewport;

    // Shadow resources (built once per frame, shared by all views).
    std::vector<FBOHandle>      fboShadow;
    std::vector<glm::mat4>      lightSpaceMatrices;
    const std::vector<RenderItem>* shadowItems = nullptr;
    const std::vector<DrawList>*   shadowLists = nullptr;

    // Shadow fit settings, so the forward pass can drive the fade uniforms.
    const ShadowFitParams* shadowFit = nullptr;

    // Per-view targets.
    FBOHandle              fboMainMS;
    FBOHandle              fboResolved;
    FBOHandle              fboSSAO;
    FBOHandle              fboSSAOBlur;
    FBOHandle              fboLit;
    std::vector<FBOHandle> fboBloom;

    std::uint32_t fullscreenVAO = 0;
    int           msaaSamples   = 4;
};
