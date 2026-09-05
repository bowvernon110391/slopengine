#pragma once

// Minimal OpenGL 3.3 Core Profile loader built on top of SDL2.
//
// We intentionally do NOT include <GL/gl.h> or use GLEW/GLAD/gl3w. Instead we
// declare exactly the subset of the OpenGL API this demo uses and resolve the
// function pointers at runtime through SDL_GL_GetProcAddress. This keeps the
// project free of extra dependencies while still being fully "core profile".
//
// Everything lives in the `gl` namespace so it never collides with the macros
// and types a real GL header might define elsewhere.

#include <SDL.h>
#include <cstddef>

#if defined(_WIN32)
#  define GL_CALL __stdcall
#else
#  define GL_CALL
#endif

namespace gl {

// ---------------------------------------------------------------------------
// Basic types
// ---------------------------------------------------------------------------
using GLenum     = unsigned int;
using GLuint     = unsigned int;
using GLint      = int;
using GLsizei    = int;
using GLboolean  = unsigned char;
using GLchar     = char;
using GLfloat    = float;
using GLubyte    = unsigned char;
using GLbitfield = unsigned int;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr   = std::ptrdiff_t;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
constexpr GLenum      COLOR_BUFFER_BIT        = 0x00004000;
constexpr GLenum      DEPTH_BUFFER_BIT        = 0x00000100;
constexpr GLenum      DEPTH_TEST              = 0x0B71;
constexpr GLenum      CULL_FACE               = 0x0B44;
constexpr GLenum      BACK                    = 0x0405;
constexpr GLenum      FRONT                   = 0x0404;
constexpr GLenum      CCW                     = 0x0901;
constexpr GLenum      CW                      = 0x0900;
constexpr GLenum      LEQUAL                  = 0x0203;
constexpr GLenum      FLOAT                   = 0x1406;
constexpr GLenum      UNSIGNED_INT            = 0x1405;
constexpr GLenum      TRIANGLES               = 0x0004;
constexpr GLenum      STATIC_DRAW             = 0x88E4;
constexpr GLenum      ARRAY_BUFFER            = 0x8892;
constexpr GLenum      ELEMENT_ARRAY_BUFFER    = 0x8893;
constexpr GLenum      VERTEX_SHADER           = 0x8B31;
constexpr GLenum      FRAGMENT_SHADER         = 0x8B30;
constexpr GLenum      COMPILE_STATUS          = 0x8B81;
constexpr GLenum      LINK_STATUS             = 0x8B82;
constexpr GLenum      FRONT_AND_BACK          = 0x0408;
constexpr GLenum      LINE                    = 0x1B01;
constexpr GLenum      FILL                    = 0x1B02;
constexpr GLenum      VERSION                 = 0x1F02;
constexpr GLenum      RENDERER                = 0x1F01;
constexpr GLenum      SHADING_LANGUAGE_VERSION = 0x8B8C;

// ---------------------------------------------------------------------------
// Function pointer declarations
// ---------------------------------------------------------------------------
#define GL_FUNC_LIST(X)                                                     \
    X(CreateShader,           GLuint,        (GLenum type))                  \
    X(ShaderSource,           void,          (GLuint shader, GLsizei count,  \
                                             const GLchar* const* string,    \
                                             const GLint* length))           \
    X(CompileShader,          void,          (GLuint shader))                \
    X(GetShaderiv,            void,          (GLuint shader, GLenum pname,   \
                                             GLint* params))                 \
    X(GetShaderInfoLog,       void,          (GLuint shader, GLsizei bufSize,\
                                             GLsizei* length, GLchar* infoLog))\
    X(DeleteShader,           void,          (GLuint shader))                \
    X(CreateProgram,          GLuint,        (void))                         \
    X(AttachShader,           void,          (GLuint program, GLuint shader))\
    X(LinkProgram,            void,          (GLuint program))               \
    X(GetProgramiv,           void,          (GLuint program, GLenum pname,  \
                                             GLint* params))                 \
    X(GetProgramInfoLog,      void,          (GLuint program, GLsizei bufSize,\
                                             GLsizei* length, GLchar* infoLog))\
    X(UseProgram,             void,          (GLuint program))               \
    X(DeleteProgram,          void,          (GLuint program))               \
    X(GenVertexArrays,        void,          (GLsizei n, GLuint* arrays))    \
    X(DeleteVertexArrays,     void,          (GLsizei n, const GLuint* arrays))\
    X(BindVertexArray,        void,          (GLuint array))                 \
    X(GenBuffers,             void,          (GLsizei n, GLuint* buffers))   \
    X(DeleteBuffers,          void,          (GLsizei n, const GLuint* buffers))\
    X(BindBuffer,             void,          (GLenum target, GLuint buffer)) \
    X(BufferData,             void,          (GLenum target, GLsizeiptr size,\
                                             const void* data, GLenum usage))\
    X(VertexAttribPointer,    void,          (GLuint index, GLint size,      \
                                             GLenum type, GLboolean normalized,\
                                             GLsizei stride, const void* ptr))\
    X(EnableVertexAttribArray,void,          (GLuint index))                 \
    X(GetAttribLocation,      GLint,         (GLuint program, const GLchar* name))\
    X(GetUniformLocation,     GLint,         (GLuint program, const GLchar* name))\
    X(UniformMatrix4fv,       void,          (GLint location, GLsizei count, \
                                             GLboolean transpose,            \
                                             const GLfloat* value))          \
    X(Uniform3fv,             void,          (GLint location, GLsizei count, \
                                             const GLfloat* value))          \
    X(Uniform1f,              void,          (GLint location, GLfloat v0))   \
    X(Uniform1i,              void,          (GLint location, GLint v0))     \
    X(Clear,                  void,          (GLbitfield mask))              \
    X(ClearColor,             void,          (GLfloat r, GLfloat g,          \
                                             GLfloat b, GLfloat a))          \
    X(Viewport,               void,          (GLint x, GLint y, GLsizei w,   \
                                             GLsizei h))                     \
    X(Enable,                 void,          (GLenum cap))                   \
    X(Disable,                void,          (GLenum cap))                   \
    X(DepthFunc,              void,          (GLenum func))                  \
    X(DepthMask,              void,          (GLboolean flag))               \
    X(CullFace,               void,          (GLenum mode))                  \
    X(FrontFace,              void,          (GLenum mode))                  \
    X(DrawArrays,             void,          (GLenum mode, GLint first,      \
                                             GLsizei count))                 \
    X(DrawElements,           void,          (GLenum mode, GLsizei count,    \
                                             GLenum type, const void* indices))\
    X(PolygonMode,            void,          (GLenum face, GLenum mode))     \
    X(GetString,              const GLubyte*, (GLenum name))

#define GL_DECLARE_TYPE(name, ret, params) using name##_t = ret(GL_CALL*) params;
#define GL_DECLARE_PTR(name, ret, params)  extern name##_t name;

GL_FUNC_LIST(GL_DECLARE_TYPE)
GL_FUNC_LIST(GL_DECLARE_PTR)

#undef GL_DECLARE_TYPE
#undef GL_DECLARE_PTR

} // namespace gl

// Resolve every function pointer; returns false (and logs) on the first failure.
bool loadGLFunctions();
