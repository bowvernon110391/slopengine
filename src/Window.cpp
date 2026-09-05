#include "Window.h"

Window::Window(const char* title, int width, int height)
    : m_width(width), m_height(height)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    // Request an OpenGL 3.3 Core Profile context.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow(title,
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                width, height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
                                SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return;
    }

    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        SDL_Log("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        return;
    }

    // Vertical sync (1 = on).
    SDL_GL_SetSwapInterval(1);

    m_ok = true;
}

Window::~Window()
{
    if (m_context) SDL_GL_DeleteContext(m_context);
    if (m_window)  SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool Window::pollEvents()
{
    m_mouseDX = 0.0f;
    m_mouseDY = 0.0f;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (m_eventHook) m_eventHook(e);
        switch (e.type) {
        case SDL_QUIT:
            m_quit = true;
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                m_width  = e.window.data1;
                m_height = e.window.data2;
            }
            break;
        case SDL_MOUSEMOTION:
            m_mouseDX += static_cast<float>(e.motion.xrel);
            m_mouseDY += static_cast<float>(e.motion.yrel);
            break;
        case SDL_KEYDOWN:
            if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) m_quit = true;
            break;
        default:
            break;
        }
    }

    m_keys = SDL_GetKeyboardState(nullptr);
    return !m_quit;
}

void Window::swap()
{
    SDL_GL_SwapWindow(m_window);
}

void Window::setRelativeMouseMode(bool enabled)
{
    SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
}

bool Window::keyDown(SDL_Scancode sc) const
{
    return m_keys && m_keys[sc];
}

void Window::mouseDelta(float& dx, float& dy) const
{
    dx = m_mouseDX;
    dy = m_mouseDY;
}
