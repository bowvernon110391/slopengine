#pragma once

#include <cstdint>
#include <vector>

#include "GL.h"
#include "core/Types.h"

// Description of a render target. colorFormat == 0 means "depth only".
struct RenderTargetDesc
{
    int         width          = 0;
    int         height         = 0;
    gl::GLenum  colorFormat    = 0;      // e.g. gl::RGBA8, or 0 for none
    gl::GLenum  depthFormat    = 0;      // e.g. gl::DEPTH_COMPONENT24, or 0
    bool        depthAsTexture = false;  // true => sampleable depth texture
    int         samples        = 0;      // 0 => single-sample

    bool operator==(const RenderTargetDesc& o) const
    {
        return width == o.width && height == o.height
            && colorFormat == o.colorFormat && depthFormat == o.depthFormat
            && depthAsTexture == o.depthAsTexture && samples == o.samples;
    }
    bool operator!=(const RenderTargetDesc& o) const { return !(*this == o); }
};

// Caches framebuffers keyed by descriptor. acquire() reuses a matching target
// that has not been used yet this frame (so multiple views get distinct
// targets); releaseAll() frees targets untouched by the current frame.
class RenderTargetPool
{
public:
    RenderTargetPool() = default;
    ~RenderTargetPool();

    RenderTargetPool(const RenderTargetPool&) = delete;
    RenderTargetPool& operator=(const RenderTargetPool&) = delete;

    void beginFrame(std::uint32_t frameTag);
    FBOHandle acquire(const RenderTargetDesc& desc);
    void releaseAll(std::uint32_t frameTag);
    void destroyAll();

    bool        valid(FBOHandle h) const;
    gl::GLuint  fbo(FBOHandle h) const;
    gl::GLuint  colorTexture(FBOHandle h) const;
    gl::GLuint  depthTexture(FBOHandle h) const;
    int         width(FBOHandle h) const  { const Target* t = resolve(h); return t ? t->desc.width  : 0; }
    int         height(FBOHandle h) const { const Target* t = resolve(h); return t ? t->desc.height : 0; }

private:
    struct Target
    {
        RenderTargetDesc desc;
        gl::GLuint fbo      = 0;
        gl::GLuint colorRB  = 0;   // multisampled color renderbuffer
        gl::GLuint depthRB  = 0;   // depth renderbuffer
        gl::GLuint colorTex = 0;   // sampleable color texture
        gl::GLuint depthTex = 0;   // sampleable depth texture
        std::uint32_t generation = 1;
        std::uint32_t lastUsed   = 0;
        bool alive = false;
    };

    Target*       resolve(FBOHandle h);
    const Target* resolve(FBOHandle h) const;

    static void createTarget(Target& t, const RenderTargetDesc& desc, std::uint32_t generation);
    static void destroyTarget(Target& t);

    std::vector<Target> m_targets;   // slot 0 unused; handle.id == slot index
    std::uint32_t       m_currentFrame = 1;
};
