#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "GL.h"
#include "renderer/FrameGraph.h"
#include "renderer/RenderQueue.h"
#include "renderer/RenderTargetPool.h"
#include "renderer/ResourceCaches.h"
#include "renderer/ShaderCache.h"
#include "renderer/passes/DebugAABBPass.h"
#include "renderer/passes/FinalPostPass.h"
#include "renderer/passes/ForwardPass.h"
#include "renderer/passes/PostStubs.h"
#include "renderer/passes/ResolvePass.h"
#include "renderer/passes/ShadowPass.h"
#include "renderer/passes/UIPass.h"

class Scene;

// One shadow map as surfaced to the debug UI. 'texture' is a grayscale preview
// rendered from the map's depth texture. It stays valid across frames because
// the preview target is owned by the Renderer rather than the frame pool.
struct ShadowMapView
{
    gl::GLuint texture     = 0;   // 0 until the first preview has been rendered
    int        previewSize = 0;
    int        sourceSize  = 0;   // shadow map resolution
};

// Owns the FrameGraph, the render-target pool, the shared shader/mesh/material
// caches, the shadow maps and the full-screen VAO; drives a frame.
class Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // shaderBasePath must end with a path separator (SDL_GetBasePath()).
    bool init(const std::string& shaderBasePath,
              int shadowMapSize = 2048,
              int msaaSamples   = 4);
    void shutdown();

    void renderFrame(const std::vector<RenderView>& views, Scene& scene);

    // Shared resources.
    MeshCache&       meshes()    { return m_meshes; }
    MaterialCache&   materials() { return m_materials; }
    ShaderCache&     shaders()   { return m_shaders; }
    RenderTargetPool& pool()     { return m_pool; }
    RenderQueue&     shadowQueue() { return m_shadowQueue; }
    FrameGraph&      frameGraph()  { return m_frameGraph; }

    // Passes.
    ShadowPass&        shadowPass()        { return *m_shadowPass; }
    ForwardPass&       forwardPass()       { return *m_forwardPass; }
    DebugAABBPass&     debugAABBPass()     { return *m_debugAABBPass; }
    ResolvePass&       resolvePass()       { return *m_resolvePass; }
    FinalPostPass&     finalPostPass()     { return *m_finalPostPass; }
    UIPass&            uiPass()            { return *m_uiPass; }
    SSAOPass&          ssaoPass()          { return *m_ssaoPass; }
    AOCompositePass&   aoCompositePass()   { return *m_aoCompositePass; }
    BloomBrightPass&   bloomBrightPass()   { return *m_bloomBrightPass; }
    BloomDownsamplePass& bloomDownsamplePass() { return *m_bloomDownsamplePass; }
    BloomUpsamplePass&   bloomUpsamplePass()   { return *m_bloomUpsamplePass; }

    gl::GLuint fullscreenVAO() const { return m_fullscreenVAO; }
    int        shadowMapSize() const { return m_shadowMapSize; }
    int        msaaSamples()   const { return m_msaaSamples; }

    // Debug overlay: wireframe world-space AABBs, drawn after the forward pass.
    void setDebugAABBs(bool enabled) { m_debugAABBs = enabled; }
    bool debugAABBs() const { return m_debugAABBs; }

    // --- Shadow map debug -------------------------------------------------
    // While enabled, each shadow map's depth texture is visualised into a
    // persistent preview target once per frame so the UI can display it.
    void setShadowDebugEnabled(bool enabled) { m_shadowDebugEnabled = enabled; }
    bool shadowDebugEnabled() const { return m_shadowDebugEnabled; }

    void  setShadowDebugRange(float minDepth, float maxDepth, bool invert);
    float shadowDebugMin() const { return m_shadowDebugMin; }
    float shadowDebugMax() const { return m_shadowDebugMax; }
    bool  shadowDebugInvert() const { return m_shadowDebugInvert; }

    const std::vector<ShadowMapView>& shadowMapViews() const { return m_shadowViews; }

    // Called once per frame by FrameGraph with the shadow targets it built.
    void setFrameShadowMaps(const std::vector<FBOHandle>& fbos);

    // Renders the grayscale preview of each shadow map. No-op when the debug
    // flag is off, so the cost is opt-in.
    void renderShadowPreviews();

    // Draw statistics for the most recent renderFrame(), summed over its views.
    void resetFrameStats() { m_statVisible = m_statCulled = m_statDraws = 0; }
    void addViewStats(std::size_t visible, std::size_t culled, std::size_t draws)
    {
        m_statVisible += visible;
        m_statCulled  += culled;
        m_statDraws   += draws;
    }
    std::size_t statVisible() const { return m_statVisible; }
    std::size_t statCulled()  const { return m_statCulled; }
    std::size_t statDraws()   const { return m_statDraws; }

    void setOutputSize(int width, int height) { m_outputWidth = width; m_outputHeight = height; }
    int  outputWidth()  const { return m_outputWidth; }
    int  outputHeight() const { return m_outputHeight; }

    bool initialized() const { return m_initialized; }

private:
    // A persistent colour target used to visualise one shadow map. Owned here
    // rather than by the pool: the pool releases and recycles targets every
    // frame, and ImGui holds the texture id between frames.
    struct ShadowPreviewTarget
    {
        gl::GLuint fbo     = 0;
        gl::GLuint texture = 0;
        int        size    = 0;
    };

    // Creates/destroys preview targets so there is one per shadow map.
    void ensureShadowPreviews(std::size_t count, int size);

    FrameGraph       m_frameGraph;
    RenderTargetPool m_pool;
    ShaderCache      m_shaders;
    MeshCache        m_meshes;
    MaterialCache    m_materials;
    RenderQueue      m_shadowQueue;

    std::unique_ptr<ShadowPass>          m_shadowPass;
    std::unique_ptr<ForwardPass>         m_forwardPass;
    std::unique_ptr<DebugAABBPass>       m_debugAABBPass;
    std::unique_ptr<ResolvePass>         m_resolvePass;
    std::unique_ptr<FinalPostPass>       m_finalPostPass;
    std::unique_ptr<UIPass>              m_uiPass;
    std::unique_ptr<SSAOPass>            m_ssaoPass;
    std::unique_ptr<AOCompositePass>     m_aoCompositePass;
    std::unique_ptr<BloomBrightPass>     m_bloomBrightPass;
    std::unique_ptr<BloomDownsamplePass> m_bloomDownsamplePass;
    std::unique_ptr<BloomUpsamplePass>   m_bloomUpsamplePass;

    gl::GLuint    m_fullscreenVAO = 0;
    std::uint32_t m_frameTag      = 1;
    int           m_outputWidth   = 0;
    int           m_outputHeight  = 0;
    int           m_shadowMapSize = 2048;
    int           m_msaaSamples   = 4;
    bool          m_debugAABBs    = false;
    bool          m_initialized   = false;

    std::size_t   m_statVisible   = 0;
    std::size_t   m_statCulled    = 0;
    std::size_t   m_statDraws     = 0;

    // Shadow-map debug state.
    std::vector<FBOHandle>           m_frameShadowFBOs;
    std::vector<ShadowMapView>       m_shadowViews;
    std::vector<ShadowPreviewTarget> m_shadowPreviews;
    Shader*                          m_depthPreviewShader = nullptr;
    bool                             m_shadowDebugEnabled = false;
    float                            m_shadowDebugMin     = 0.0f;
    float                            m_shadowDebugMax     = 1.0f;
    bool                             m_shadowDebugInvert  = false;
};
