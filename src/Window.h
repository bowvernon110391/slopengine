#pragma once

#include <SDL.h>

// RAII wrapper around an SDL window + OpenGL context.
class Window
{
public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ok() const { return m_ok; }

    // Pump all pending SDL events. Returns false when a quit was requested.
    bool pollEvents();

    void swap();
    void setRelativeMouseMode(bool enabled);

    int width() const  { return m_width; }
    int height() const { return m_height; }

    bool keyDown(SDL_Scancode sc) const;
    void mouseDelta(float& dx, float& dy) const;

    // SDL handles needed to initialise ImGui's SDL2 + OpenGL3 backends.
    SDL_Window*   sdlWindow() const { return m_window; }
    SDL_GLContext glContext() const { return m_context; }

    // Optional hook invoked for every SDL event (used to feed ImGui).
    void setEventHook(void (*hook)(const SDL_Event&)) { m_eventHook = hook; }

private:
    SDL_Window*   m_window = nullptr;
    SDL_GLContext m_context = nullptr;

    void (*m_eventHook)(const SDL_Event&) = nullptr;

    int  m_width = 0;
    int  m_height = 0;
    bool m_ok = false;
    bool m_quit = false;

    float m_mouseDX = 0.0f;
    float m_mouseDY = 0.0f;

    const Uint8* m_keys = nullptr;
};
