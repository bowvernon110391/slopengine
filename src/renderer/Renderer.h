#pragma once

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
#include "renderer/passes/FinalPostPass.h"
#include "renderer/passes/ForwardPass.h"
#include "renderer/passes/PostStubs.h"
#include "renderer/passes/ResolvePass.h"
#include "renderer/passes/ShadowPass.h"
#include "renderer/passes/UIPass.h"

class Scene;

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

    void setOutputSize(int width, int height) { m_outputWidth = width; m_outputHeight = height; }
    int  outputWidth()  const { return m_outputWidth; }
    int  outputHeight() const { return m_outputHeight; }

    bool initialized() const { return m_initialized; }

private:
    FrameGraph       m_frameGraph;
    RenderTargetPool m_pool;
    ShaderCache      m_shaders;
    MeshCache        m_meshes;
    MaterialCache    m_materials;
    RenderQueue      m_shadowQueue;

    std::unique_ptr<ShadowPass>          m_shadowPass;
    std::unique_ptr<ForwardPass>         m_forwardPass;
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
    bool          m_initialized   = false;
};
