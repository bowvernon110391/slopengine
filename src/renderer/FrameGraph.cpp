#include "renderer/FrameGraph.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/RenderContext.h"
#include "renderer/RenderTargetPool.h"
#include "renderer/Renderer.h"
#include "scene/Scene.h"

namespace {

std::vector<Light> gatherLights(const Scene& scene)
{
    std::vector<Light> lights;
    const ComponentStorage<LightComponent>& storage = scene.lights();
    for (const Entity& e : storage.entities()) {
        Light l;
        if (scene.makeLight(e, l)) lights.push_back(l);
    }
    return lights;
}

// Orthographic light-space matrix fitted to the scene bounds.
glm::mat4 directionalLightMatrix(const Light& light, const AABB& bounds)
{
    const glm::vec3 center = bounds.valid() ? bounds.center() : glm::vec3(0.0f);
    float radius = bounds.valid() ? bounds.radius() : 10.0f;
    if (radius < 1.0f) radius = 1.0f;
    radius *= 1.25f;

    glm::vec3 dir = light.direction;
    if (glm::dot(dir, dir) < 1e-6f) dir = glm::vec3(0.0f, -1.0f, 0.0f);
    dir = glm::normalize(dir);

    const glm::vec3 up = (std::fabs(dir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f)
                                                    : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 eye = center - dir * (radius * 2.0f);

    const glm::mat4 view = glm::lookAt(eye, center, up);
    const glm::mat4 proj = glm::ortho(-radius, radius, -radius, radius,
                                      0.1f, radius * 4.0f + 2.0f);
    return proj * view;
}

void acquireViewResources(Renderer& r, RenderView& v)
{
    int w = v.viewport.width > 0 ? v.viewport.width : r.outputWidth();
    int h = v.viewport.height > 0 ? v.viewport.height : r.outputHeight();
    if (w <= 0 || h <= 0) return;

    v.viewport.width  = w;
    v.viewport.height = h;

    const int samples = r.msaaSamples();
    RenderTargetDesc mainDesc;
    mainDesc.width = w; mainDesc.height = h;
    mainDesc.colorFormat = gl::RGBA8;
    mainDesc.depthFormat = gl::DEPTH_COMPONENT24;
    mainDesc.depthAsTexture = false;
    mainDesc.samples = samples;
    v.resources.fboMainMS = r.pool().acquire(mainDesc);

    RenderTargetDesc resolveDesc;
    resolveDesc.width = w; resolveDesc.height = h;
    resolveDesc.colorFormat = gl::RGBA8;
    v.resources.fboResolved = r.pool().acquire(resolveDesc);

    v.resources.width  = w;
    v.resources.height = h;
}

void runPass(RenderPass& pass, RenderContext& ctx, bool enabled)
{
    if (!enabled) return;
    const bool ok = pass.setup(ctx);
    if (ok) pass.execute(ctx);
    pass.teardown(ctx);
}

} // namespace

void FrameGraph::sortViews()
{
    std::stable_sort(m_views.begin(), m_views.end(),
                     [](const RenderView& a, const RenderView& b) {
                         return a.order < b.order;
                     });
}

void FrameGraph::render(Renderer& renderer, Scene& scene)
{
    sortViews();

    const std::vector<Light> lights = gatherLights(scene);

    // --- Shadow pass (once per frame, shared by all views) ---------------
    renderer.shadowQueue().buildShadowLists(scene, lights,
                                            renderer.meshes(), renderer.materials(),
                                            0xFFFFFFFFu);

    RenderContext shadowCtx;
    shadowCtx.scene        = &scene;
    shadowCtx.lights       = &lights;
    shadowCtx.meshes       = &renderer.meshes();
    shadowCtx.materials    = &renderer.materials();
    shadowCtx.shaders      = &renderer.shaders();
    shadowCtx.pool         = &renderer.pool();
    shadowCtx.fullscreenVAO = renderer.fullscreenVAO();
    shadowCtx.shadowItems  = &renderer.shadowQueue().frameItems();
    shadowCtx.shadowLists  = &renderer.shadowQueue().shadowLists();

    int shadowLightCount = 0;
    for (const Light& l : lights) if (l.castsShadow) ++shadowLightCount;

    for (int i = 0; i < shadowLightCount; ++i) {
        RenderTargetDesc desc;
        desc.width  = renderer.shadowMapSize();
        desc.height = renderer.shadowMapSize();
        desc.colorFormat = 0;                        // depth only
        desc.depthFormat = gl::DEPTH_COMPONENT24;
        desc.depthAsTexture = true;                  // sampled by the forward pass
        desc.samples = 0;
        shadowCtx.fboShadow.push_back(renderer.pool().acquire(desc));
    }

    for (const Light& l : lights) {
        if (!l.castsShadow) continue;
        shadowCtx.lightSpaceMatrices.push_back(directionalLightMatrix(l, scene.worldBounds()));
    }

    if (!shadowCtx.fboShadow.empty()) {
        runPass(renderer.shadowPass(), shadowCtx, true);
    }

    // --- Per-view passes --------------------------------------------------
    for (RenderView& view : m_views) {
        view.lights = lights;
        Scene* viewScene = view.scene ? view.scene : &scene;

        acquireViewResources(renderer, view);
        view.queue.buildCameraLists(*viewScene, view.camera,
                                    renderer.meshes(), renderer.materials(),
                                    view.layerMask);

        RenderContext ctx;
        ctx.queue        = &view.queue;
        ctx.view         = &view;
        ctx.scene        = viewScene;
        ctx.lights       = &view.lights;
        ctx.meshes       = &renderer.meshes();
        ctx.materials    = &renderer.materials();
        ctx.shaders      = &renderer.shaders();
        ctx.pool         = &renderer.pool();
        ctx.viewport     = view.viewport;
        ctx.fboShadow    = shadowCtx.fboShadow;
        ctx.lightSpaceMatrices = shadowCtx.lightSpaceMatrices;
        ctx.shadowItems  = &renderer.shadowQueue().frameItems();
        ctx.shadowLists  = &renderer.shadowQueue().shadowLists();
        ctx.fboMainMS    = view.resources.fboMainMS;
        ctx.fboResolved  = view.resources.fboResolved;
        ctx.fboSSAO      = view.resources.fboSSAO;
        ctx.fboSSAOBlur  = view.resources.fboSSAOBlur;
        ctx.fboLit       = view.resources.fboLit;
        ctx.fboBloom     = view.resources.fboBloom;
        ctx.fullscreenVAO = renderer.fullscreenVAO();
        ctx.msaaSamples  = renderer.msaaSamples();

        runPass(renderer.forwardPass(),   ctx, (view.passes & Pass_Forward)   != 0);
        runPass(renderer.resolvePass(),   ctx, (view.passes & Pass_Resolve)   != 0);
        // SSAO / Bloom are wired but disabled: targets are not acquired and
        // their pass bits are off in the default mask.
        runPass(renderer.ssaoPass(),      ctx, (view.passes & Pass_SSAO)      != 0);
        runPass(renderer.aoCompositePass(), ctx, (view.passes & Pass_AOComposite) != 0);
        runPass(renderer.bloomBrightPass(),   ctx, (view.passes & Pass_Bloom) != 0);
        runPass(renderer.bloomDownsamplePass(), ctx, (view.passes & Pass_Bloom) != 0);
        runPass(renderer.bloomUpsamplePass(),   ctx, (view.passes & Pass_Bloom) != 0);
        runPass(renderer.finalPostPass(), ctx, (view.passes & Pass_FinalPost) != 0);
        runPass(renderer.uiPass(),        ctx, (view.passes & Pass_UI)        != 0);
    }
}
