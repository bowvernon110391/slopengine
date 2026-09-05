#include "GL.h"

namespace gl {

#define GL_DEFINE_PTR(name, ret, params) name##_t name = nullptr;
GL_FUNC_LIST(GL_DEFINE_PTR)
#undef GL_DEFINE_PTR

} // namespace gl

bool loadGLFunctions()
{
#define GL_LOAD(name, ret, params)                                  \
    gl::name = reinterpret_cast<gl::name##_t>(                      \
        SDL_GL_GetProcAddress("gl" #name));                         \
    if (gl::name == nullptr) {                                      \
        SDL_Log("Failed to load OpenGL function: %s", "gl" #name);  \
        return false;                                               \
    }

    GL_FUNC_LIST(GL_LOAD)
#undef GL_LOAD

    return true;
}
