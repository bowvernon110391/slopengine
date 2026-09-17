#include "renderer/RenderTargetPool.h"

#include <cstddef>

RenderTargetPool::~RenderTargetPool()
{
    destroyAll();
}

RenderTargetPool::Target* RenderTargetPool::resolve(FBOHandle h)
{
    if (!h.valid() || h.id >= m_targets.size()) return nullptr;
    Target& t = m_targets[h.id];
    if (!t.alive || t.generation != h.generation) return nullptr;
    return &t;
}

const RenderTargetPool::Target* RenderTargetPool::resolve(FBOHandle h) const
{
    return const_cast<RenderTargetPool*>(this)->resolve(h);
}

bool        RenderTargetPool::valid(FBOHandle h) const        { return resolve(h) != nullptr; }
gl::GLuint  RenderTargetPool::fbo(FBOHandle h) const          { const Target* t = resolve(h); return t ? t->fbo      : 0; }
gl::GLuint  RenderTargetPool::colorTexture(FBOHandle h) const { const Target* t = resolve(h); return t ? t->colorTex : 0; }
gl::GLuint  RenderTargetPool::depthTexture(FBOHandle h) const { const Target* t = resolve(h); return t ? t->depthTex : 0; }

void RenderTargetPool::beginFrame(std::uint32_t frameTag)
{
    m_currentFrame = frameTag;
}

namespace {

void texImageFormat(gl::GLenum internalFormat, gl::GLenum& format, gl::GLenum& type)
{
    switch (internalFormat) {
    case gl::RGBA16F: format = gl::RGBA; type = gl::FLOAT;          break;
    case gl::RGB8:    format = gl::RGB;  type = gl::UNSIGNED_BYTE;  break;
    default:          format = gl::RGBA; type = gl::UNSIGNED_BYTE;  break;
    }
}

} // namespace

void RenderTargetPool::createTarget(Target& t, const RenderTargetDesc& desc, std::uint32_t generation)
{
    t.desc       = desc;
    t.generation = generation;
    t.alive      = true;
    t.lastUsed   = 0;

    gl::GenFramebuffers(1, &t.fbo);
    gl::BindFramebuffer(gl::FRAMEBUFFER, t.fbo);

    if (desc.colorFormat != 0) {
        if (desc.samples > 0) {
            gl::GenRenderbuffers(1, &t.colorRB);
            gl::BindRenderbuffer(gl::RENDERBUFFER, t.colorRB);
            gl::RenderbufferStorageMultisample(gl::RENDERBUFFER, desc.samples,
                                               desc.colorFormat, desc.width, desc.height);
            gl::FramebufferRenderbuffer(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0,
                                        gl::RENDERBUFFER, t.colorRB);
        } else {
            gl::GLenum format = gl::RGBA, type = gl::UNSIGNED_BYTE;
            texImageFormat(desc.colorFormat, format, type);
            gl::GenTextures(1, &t.colorTex);
            gl::BindTexture(gl::TEXTURE_2D, t.colorTex);
            gl::TexImage2D(gl::TEXTURE_2D, 0, static_cast<gl::GLint>(desc.colorFormat),
                           desc.width, desc.height, 0, format, type, nullptr);
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, static_cast<gl::GLint>(gl::LINEAR));
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, static_cast<gl::GLint>(gl::LINEAR));
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, static_cast<gl::GLint>(gl::CLAMP_TO_EDGE));
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, static_cast<gl::GLint>(gl::CLAMP_TO_EDGE));
            gl::FramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0,
                                     gl::TEXTURE_2D, t.colorTex, 0);
        }
        const gl::GLenum drawBuf = gl::COLOR_ATTACHMENT0;
        gl::DrawBuffers(1, &drawBuf);
    } else {
        gl::DrawBuffer(gl::NONE);
        gl::ReadBuffer(gl::NONE);
    }

    if (desc.depthFormat != 0) {
        if (desc.depthAsTexture) {
            gl::GenTextures(1, &t.depthTex);
            gl::BindTexture(gl::TEXTURE_2D, t.depthTex);
            gl::TexImage2D(gl::TEXTURE_2D, 0, static_cast<gl::GLint>(desc.depthFormat),
                           desc.width, desc.height, 0, gl::DEPTH_COMPONENT, gl::FLOAT, nullptr);
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, static_cast<gl::GLint>(gl::NEAREST));
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, static_cast<gl::GLint>(gl::NEAREST));
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, static_cast<gl::GLint>(gl::CLAMP_TO_EDGE));
            gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, static_cast<gl::GLint>(gl::CLAMP_TO_EDGE));
            gl::FramebufferTexture2D(gl::FRAMEBUFFER, gl::DEPTH_ATTACHMENT,
                                     gl::TEXTURE_2D, t.depthTex, 0);
        } else {
            gl::GenRenderbuffers(1, &t.depthRB);
            gl::BindRenderbuffer(gl::RENDERBUFFER, t.depthRB);
            if (desc.samples > 0) {
                gl::RenderbufferStorageMultisample(gl::RENDERBUFFER, desc.samples,
                                                   desc.depthFormat, desc.width, desc.height);
            } else {
                gl::RenderbufferStorage(gl::RENDERBUFFER, desc.depthFormat, desc.width, desc.height);
            }
            gl::FramebufferRenderbuffer(gl::FRAMEBUFFER, gl::DEPTH_ATTACHMENT,
                                        gl::RENDERBUFFER, t.depthRB);
        }
    }

    const gl::GLenum status = gl::CheckFramebufferStatus(gl::FRAMEBUFFER);
    if (status != gl::FRAMEBUFFER_COMPLETE) {
        SDL_Log("RenderTargetPool: incomplete framebuffer (%dx%d, status 0x%X)",
                desc.width, desc.height, static_cast<unsigned>(status));
    }

    gl::BindFramebuffer(gl::FRAMEBUFFER, 0);
}

void RenderTargetPool::destroyTarget(Target& t)
{
    if (t.depthTex) { gl::DeleteTextures(1, &t.depthTex); t.depthTex = 0; }
    if (t.colorTex) { gl::DeleteTextures(1, &t.colorTex); t.colorTex = 0; }
    if (t.depthRB)  { gl::DeleteRenderbuffers(1, &t.depthRB); t.depthRB = 0; }
    if (t.colorRB)  { gl::DeleteRenderbuffers(1, &t.colorRB); t.colorRB = 0; }
    if (t.fbo)      { gl::DeleteFramebuffers(1, &t.fbo); t.fbo = 0; }
    t.alive = false;
}

FBOHandle RenderTargetPool::acquire(const RenderTargetDesc& desc)
{
    // Reuse a live target with a matching descriptor that has not been handed
    // out yet this frame.
    for (std::size_t i = 1; i < m_targets.size(); ++i) {
        Target& t = m_targets[i];
        if (t.alive && t.lastUsed != m_currentFrame && t.desc == desc) {
            t.lastUsed = m_currentFrame;
            FBOHandle h; h.id = static_cast<std::uint32_t>(i); h.generation = t.generation;
            return h;
        }
    }

    // Otherwise take a dead slot, or grow the pool.
    std::size_t slot = 0;
    for (std::size_t i = 1; i < m_targets.size(); ++i) {
        if (!m_targets[i].alive) { slot = i; break; }
    }
    if (slot == 0) {
        m_targets.push_back(Target{});
        slot = m_targets.size() - 1;
    }

    Target& t = m_targets[slot];
    const std::uint32_t generation = t.generation + 1;   // bump on every (re)creation
    createTarget(t, desc, generation);
    t.lastUsed = m_currentFrame;

    FBOHandle h; h.id = static_cast<std::uint32_t>(slot); h.generation = generation;
    return h;
}

void RenderTargetPool::releaseAll(std::uint32_t frameTag)
{
    for (std::size_t i = 1; i < m_targets.size(); ++i) {
        Target& t = m_targets[i];
        if (t.alive && t.lastUsed != frameTag) destroyTarget(t);
    }
}

void RenderTargetPool::destroyAll()
{
    for (std::size_t i = 1; i < m_targets.size(); ++i) {
        if (m_targets[i].alive) destroyTarget(m_targets[i]);
    }
    m_targets.clear();
}
