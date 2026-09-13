#include <cmath>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "Camera.h"
#include "Mesh.h"
#include "Shader.h"
#include "Window.h"

namespace {

constexpr float PI = 3.14159265358979323846f;

// True while the fly-camera owns the mouse (toggled with F1). This lives at
// namespace scope because Window::setEventHook() takes a plain function
// pointer, so the hook below cannot capture a local variable.
bool g_cursorCaptured = true;

// Feed SDL events to ImGui's SDL2 backend so it can track input focus.
//
// While the camera owns the mouse, mouse events are withheld entirely. In
// relative mode SDL still reports motion (pinned near the window centre) along
// with button and wheel events, which would otherwise let the GUI hover, drag
// and scroll behind the camera's back.
void imguiEventHook(const SDL_Event& e)
{
    if (g_cursorCaptured) {
        switch (e.type) {
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            return;
        default:
            break;
        }
    }
    ImGui_ImplSDL2_ProcessEvent(&e);
}

// Remove every trace of mouse state from ImGui for this frame.
//
// Must be called after ImGui::NewFrame() -- which flushes the queued backend
// events into io -- and before the first widget, so the whole frame is inert.
//
// ClearInputMouse() covers position, buttons, wheel and down-durations. The
// click-tracking fields below are not covered by it. The timestamps in
// particular must be sent back to a "never clicked" value: UpdateMouseInputs()
// treats a click landing within MouseDoubleClickTime of MouseClickedTime[] as a
// repeat, so a stale pre-capture click time would turn the first click after
// releasing capture into a spurious double-click.
// (MouseDragMaxDistanceSqr[] needs no reset: IsMouseDragging/IsMouseClicked both
// bail out when MouseDown[] is false.)
void suppressImGuiMouse(ImGuiIO& io)
{
    io.ClearInputMouse();
    for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i) {
        io.MouseClicked[i]          = false;
        io.MouseClickedCount[i]     = 0;
        io.MouseClickedLastCount[i] = 0;
        io.MouseReleased[i]         = false;
        io.MouseClickedTime[i]      = -FLT_MAX;
        io.MouseReleasedTime[i]     = -FLT_MAX;
    }
    io.MouseDelta = ImVec2(0.0f, 0.0f);
}

// Append one face of the cube (two triangles) with an outward normal and a
// per-face color. Corners are wound CCW when viewed from outside, so back-face
// culling works as expected.
void pushFace(std::vector<Vertex>& out, const glm::vec3& n, const glm::vec3& color)
{
    // Pick a reference vector not parallel to the normal, then build a
    // tangent/bitangent pair that spans the face.
    glm::vec3 ref = (std::fabs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                            : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 t = glm::normalize(glm::cross(n, ref));
    glm::vec3 b = glm::cross(n, t);

    glm::vec3 c0 = (-t - b) * 0.5f;
    glm::vec3 c1 = ( t - b) * 0.5f;
    glm::vec3 c2 = ( t + b) * 0.5f;
    glm::vec3 c3 = (-t + b) * 0.5f;

    const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; ++i) {
        Vertex v;
        v.normal = n;
        v.color = color;
        switch (idx[i]) {
        case 0: v.position = c0; break;
        case 1: v.position = c1; break;
        case 2: v.position = c2; break;
        default: v.position = c3; break;
        }
        out.push_back(v);
    }
}

// A unit cube centered at the origin (side length 1).
std::vector<Vertex> makeCube()
{
    std::vector<Vertex> verts;
    pushFace(verts, glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.90f, 0.25f, 0.25f)); // front
    pushFace(verts, glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.90f, 0.55f, 0.20f)); // back
    pushFace(verts, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.20f, 0.80f, 0.25f)); // left
    pushFace(verts, glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.25f, 0.35f, 0.90f)); // right
    pushFace(verts, glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.90f, 0.90f, 0.25f)); // top
    pushFace(verts, glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.70f, 0.25f, 0.80f)); // bottom
    return verts;
}

// Append a single triangle with per-vertex normals and a shared color.
void addTriangle(std::vector<Vertex>& out,
                 const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                 const glm::vec3& na, const glm::vec3& nb, const glm::vec3& nc,
                 const glm::vec3& color)
{
    out.push_back(Vertex{a, na, color});
    out.push_back(Vertex{b, nb, color});
    out.push_back(Vertex{c, nc, color});
}

// Append a flat circular cap (triangle fan) at height y, facing +Y or -Y.
void addCap(std::vector<Vertex>& verts, int sectors, float radius, float y,
            bool up, const glm::vec3& color)
{
    glm::vec3 n(0.0f, up ? 1.0f : -1.0f, 0.0f);
    glm::vec3 center(0.0f, y, 0.0f);
    for (int s = 0; s < sectors; ++s) {
        float th0 = 2.0f * PI * static_cast<float>(s) / sectors;
        float th1 = 2.0f * PI * static_cast<float>(s + 1) / sectors;
        glm::vec3 p0(radius * std::cos(th0), y, radius * std::sin(th0));
        glm::vec3 p1(radius * std::cos(th1), y, radius * std::sin(th1));
        if (up) addTriangle(verts, center, p1, p0, n, n, n, color);
        else    addTriangle(verts, center, p0, p1, n, n, n, color);
    }
}

// A UV-sphere centered at the origin with smooth normals.
std::vector<Vertex> makeSphere(int rings, int sectors, float radius, const glm::vec3& color)
{
    std::vector<Vertex> verts;
    for (int r = 0; r < rings; ++r) {
        float phi0 = PI * static_cast<float>(r) / rings;
        float phi1 = PI * static_cast<float>(r + 1) / rings;
        for (int s = 0; s < sectors; ++s) {
            float th0 = 2.0f * PI * static_cast<float>(s) / sectors;
            float th1 = 2.0f * PI * static_cast<float>(s + 1) / sectors;

            auto point = [radius](float phi, float th) {
                return glm::vec3(radius * std::sin(phi) * std::cos(th),
                                 radius * std::cos(phi),
                                 radius * std::sin(phi) * std::sin(th));
            };
            glm::vec3 v00 = point(phi0, th0);
            glm::vec3 v01 = point(phi0, th1);
            glm::vec3 v10 = point(phi1, th0);
            glm::vec3 v11 = point(phi1, th1);

            // Skip the degenerate triangles at the poles.
            if (r != 0) {
                addTriangle(verts, v00, v01, v10,
                            v00 / radius, v01 / radius, v10 / radius, color);
            }
            if (r != rings - 1) {
                addTriangle(verts, v01, v11, v10,
                            v01 / radius, v11 / radius, v10 / radius, color);
            }
        }
    }
    return verts;
}

// A cylinder centered at the origin with its axis along Y, including flat caps.
std::vector<Vertex> makeCylinder(int sectors, float radius, float halfHeight, const glm::vec3& color)
{
    std::vector<Vertex> verts;
    const float h = halfHeight;

    for (int s = 0; s < sectors; ++s) {
        float th0 = 2.0f * PI * static_cast<float>(s) / sectors;
        float th1 = 2.0f * PI * static_cast<float>(s + 1) / sectors;

        glm::vec3 n0(std::cos(th0), 0.0f, std::sin(th0));
        glm::vec3 n1(std::cos(th1), 0.0f, std::sin(th1));

        glm::vec3 p00(radius * n0.x, -h, radius * n0.z);
        glm::vec3 p01(radius * n0.x,  h, radius * n0.z);
        glm::vec3 p10(radius * n1.x, -h, radius * n1.z);
        glm::vec3 p11(radius * n1.x,  h, radius * n1.z);

        addTriangle(verts, p00, p01, p10, n0, n0, n1, color);
        addTriangle(verts, p01, p11, p10, n0, n1, n1, color);
    }

    addCap(verts, sectors, radius, -h, false, color);
    addCap(verts, sectors, radius,  h, true,  color);
    return verts;
}

// A right cone centered at the origin, axis along Y, apex at +Y.
std::vector<Vertex> makeCone(int sectors, float radius, float height, const glm::vec3& color)
{
    std::vector<Vertex> verts;
    const float halfH = height * 0.5f;
    const float m = radius / height;                 // side slope
    const float inv = 1.0f / std::sqrt(1.0f + m * m);
    const glm::vec3 apex(0.0f, halfH, 0.0f);

    for (int s = 0; s < sectors; ++s) {
        float th0 = 2.0f * PI * static_cast<float>(s) / sectors;
        float th1 = 2.0f * PI * static_cast<float>(s + 1) / sectors;
        float thm = (th0 + th1) * 0.5f;

        glm::vec3 b0(radius * std::cos(th0), -halfH, radius * std::sin(th0));
        glm::vec3 b1(radius * std::cos(th1), -halfH, radius * std::sin(th1));

        glm::vec3 n0(std::cos(th0) * inv, m * inv, std::sin(th0) * inv);
        glm::vec3 n1(std::cos(th1) * inv, m * inv, std::sin(th1) * inv);
        glm::vec3 nA(std::cos(thm) * inv, m * inv, std::sin(thm) * inv);

        addTriangle(verts, apex, b1, b0, nA, n1, n0, color);
    }

    addCap(verts, sectors, radius, -halfH, false, color);
    return verts;
}

// A torus centered at the origin, lying in the XZ plane (hole along Y).
std::vector<Vertex> makeTorus(int majorSegs, int minorSegs,
                              float majorR, float minorR, const glm::vec3& color)
{
    std::vector<Vertex> verts;
    for (int i = 0; i < majorSegs; ++i) {
        float u0 = 2.0f * PI * static_cast<float>(i) / majorSegs;
        float u1 = 2.0f * PI * static_cast<float>(i + 1) / majorSegs;
        for (int j = 0; j < minorSegs; ++j) {
            float v0 = 2.0f * PI * static_cast<float>(j) / minorSegs;
            float v1 = 2.0f * PI * static_cast<float>(j + 1) / minorSegs;

            auto point = [majorR, minorR](float u, float v) {
                float cu = std::cos(u), su = std::sin(u);
                float cv = std::cos(v), sv = std::sin(v);
                float R = majorR + minorR * cv;
                return glm::vec3(R * cu, minorR * sv, R * su);
            };
            auto normal = [](float u, float v) {
                return glm::vec3(std::cos(v) * std::cos(u),
                                 std::sin(v),
                                 std::cos(v) * std::sin(u));
            };

            glm::vec3 p00 = point(u0, v0);
            glm::vec3 p01 = point(u0, v1);
            glm::vec3 p10 = point(u1, v0);
            glm::vec3 p11 = point(u1, v1);

            addTriangle(verts, p00, p01, p10,
                        normal(u0, v0), normal(u0, v1), normal(u1, v0), color);
            addTriangle(verts, p01, p11, p10,
                        normal(u0, v1), normal(u1, v1), normal(u1, v0), color);
        }
    }
    return verts;
}

// A checkerboard ground plane (gridSize x gridSize cells) in the XZ plane.
std::vector<Vertex> makeGround(int gridSize, float cell, float y)
{
    std::vector<Vertex> verts;
    const float half = (gridSize * cell) * 0.5f;

    for (int iz = 0; iz < gridSize; ++iz) {
        for (int ix = 0; ix < gridSize; ++ix) {
            float x0 = -half + ix * cell;
            float z0 = -half + iz * cell;
            float x1 = x0 + cell;
            float z1 = z0 + cell;

            glm::vec3 color = ((ix + iz) % 2 == 0)
                             ? glm::vec3(0.45f, 0.45f, 0.47f)
                             : glm::vec3(0.28f, 0.28f, 0.30f);

            // Wound CCW when viewed from above (+Y) so normals point up.
            verts.push_back(Vertex{glm::vec3(x0, y, z0), glm::vec3(0.0f, 1.0f, 0.0f), color});
            verts.push_back(Vertex{glm::vec3(x1, y, z1), glm::vec3(0.0f, 1.0f, 0.0f), color});
            verts.push_back(Vertex{glm::vec3(x1, y, z0), glm::vec3(0.0f, 1.0f, 0.0f), color});

            verts.push_back(Vertex{glm::vec3(x0, y, z0), glm::vec3(0.0f, 1.0f, 0.0f), color});
            verts.push_back(Vertex{glm::vec3(x0, y, z1), glm::vec3(0.0f, 1.0f, 0.0f), color});
            verts.push_back(Vertex{glm::vec3(x1, y, z1), glm::vec3(0.0f, 1.0f, 0.0f), color});
        }
    }
    return verts;
}

} // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    Window window("SDL2 + OpenGL 3.3 Demo", 800, 600);
    if (!window.ok()) {
        SDL_Log("Failed to create window / OpenGL context.");
        return 1;
    }

    if (!loadGLFunctions()) {
        SDL_Log("Failed to load OpenGL functions.");
        return 1;
    }

    SDL_Log("OpenGL Renderer: %s",
            reinterpret_cast<const char*>(gl::GetString(gl::RENDERER)));
    SDL_Log("OpenGL Version:  %s",
            reinterpret_cast<const char*>(gl::GetString(gl::VERSION)));
    SDL_Log("GLSL Version:    %s",
            reinterpret_cast<const char*>(gl::GetString(gl::SHADING_LANGUAGE_VERSION)));

    // --- ImGui setup ------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window.sdlWindow(), window.glContext());
    ImGui_ImplOpenGL3_Init("#version 330");
    window.setEventHook(imguiEventHook);

    // Shaders are copied next to the executable by CMake. The single .glsl
    // file contains both stages, and #includes reusable functions.
    std::string base = SDL_GetBasePath();
    Shader shader;
    if (!shader.load(base + "shaders/basic.glsl")) {
        return 1;
    }

    // Report the #define flags the shader source declares.
    for (const std::string& d : shader.defines()) {
        SDL_Log("Shader define: %s", d.c_str());
    }

    Mesh cube(makeCube());
    Mesh sphere(makeSphere(24, 32, 1.0f, glm::vec3(0.25f, 0.55f, 0.90f)));
    Mesh cylinder(makeCylinder(32, 0.6f, 0.6f, glm::vec3(0.90f, 0.45f, 0.20f)));
    Mesh cone(makeCone(32, 0.7f, 1.4f, glm::vec3(0.20f, 0.80f, 0.45f)));
    Mesh torus(makeTorus(32, 24, 0.5f, 0.18f, glm::vec3(0.90f, 0.30f, 0.60f)));
    Mesh ground(makeGround(24, 1.0f, -0.5f));

    Camera camera;
    camera.setAspect(static_cast<float>(window.width()) / window.height());

    // Global GL state.
    gl::Enable(gl::DEPTH_TEST);
    gl::DepthFunc(gl::LEQUAL);
    gl::Enable(gl::CULL_FACE);
    gl::CullFace(gl::BACK);
    gl::FrontFace(gl::CCW);

    window.setRelativeMouseMode(true);

    Uint64 last = SDL_GetTicks64();
    float elapsed = 0.0f;
    bool wireframe = false;
    bool wasF = false;

    // g_cursorCaptured (F1 toggles fly-camera <-> UI interaction) is declared
    // at namespace scope so imguiEventHook() can consult it too.
    bool wasF1 = false;

    float fpsSmooth = 0.0f;
    float fpsHistory[120] = {0};
    int   fpsHistoryCount = 0;

    while (window.pollEvents()) {
        Uint64 now = SDL_GetTicks64();
        float dt = static_cast<float>(now - last) / 1000.0f;
        last = now;
        if (dt > 0.25f) dt = 0.25f; // clamp the (potentially large) first frame
        elapsed += dt;

        // Toggle wireframe with F (rising-edge detection).
        bool isF = window.keyDown(SDL_SCANCODE_F);
        if (isF && !wasF) {
            wireframe = !wireframe;
            gl::PolygonMode(gl::FRONT_AND_BACK, wireframe ? gl::LINE : gl::FILL);
        }
        wasF = isF;

        // F1 toggles mouse capture so you can switch between flying the
        // camera and interacting with the ImGui windows.
        bool isF1 = window.keyDown(SDL_SCANCODE_F1);
        if (isF1 && !wasF1) {
            g_cursorCaptured = !g_cursorCaptured;
            window.setRelativeMouseMode(g_cursorCaptured);
        }
        wasF1 = isF1;

        // --- ImGui frame --------------------------------------------------
        ImGuiIO& io = ImGui::GetIO();

        // While the camera owns the mouse, tell ImGui to ignore mouse input
        // entirely for this frame. ImGui evaluates this flag in NewFrame(),
        // where it clears the hovered window and disables mouse interactions.
        if (g_cursorCaptured)
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        else
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Also wipe the accumulated mouse state (position, buttons, click
        // tracking) so no widget can hover or respond this frame.
        if (g_cursorCaptured)
            suppressImGuiMouse(io);

        // --- Camera input -------------------------------------------------
        float fwd = 0.0f, rgt = 0.0f, up = 0.0f;
        if (!io.WantCaptureKeyboard) {
            if (window.keyDown(SDL_SCANCODE_W) || window.keyDown(SDL_SCANCODE_UP))   fwd += 1.0f;
            if (window.keyDown(SDL_SCANCODE_S) || window.keyDown(SDL_SCANCODE_DOWN)) fwd -= 1.0f;
            if (window.keyDown(SDL_SCANCODE_D) || window.keyDown(SDL_SCANCODE_RIGHT)) rgt += 1.0f;
            if (window.keyDown(SDL_SCANCODE_A) || window.keyDown(SDL_SCANCODE_LEFT))  rgt -= 1.0f;
            if (window.keyDown(SDL_SCANCODE_SPACE)) up += 1.0f;
            if (window.keyDown(SDL_SCANCODE_LCTRL) || window.keyDown(SDL_SCANCODE_C)) up -= 1.0f;
            camera.processKeyboard(fwd, rgt, up, dt);
        }

        float mdx = 0.0f, mdy = 0.0f;
        window.mouseDelta(mdx, mdy);
        if (g_cursorCaptured && (mdx != 0.0f || mdy != 0.0f)) camera.processMouse(mdx, mdy);

        // --- ImGui UI (FPS counter + camera properties) --------------------
        float fps = (dt > 0.0f) ? 1.0f / dt : 0.0f;
        fpsSmooth = (fpsSmooth == 0.0f) ? fps : fpsSmooth * 0.95f + fps * 0.05f;

        fpsHistory[fpsHistoryCount % 120] = fps;
        ++fpsHistoryCount;

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performance");
        ImGui::Text("FPS: %.1f", fpsSmooth);
        ImGui::Text("Frame time: %.2f ms", dt * 1000.0f);
        int n = fpsHistoryCount < 120 ? fpsHistoryCount : 120;
        ImGui::PlotLines("##fps", fpsHistory, n, 0, nullptr, 0.0f, 240.0f, ImVec2(0, 50));
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(10, 110), ImGuiCond_FirstUseEver);
        ImGui::Begin("Camera");
        ImGui::TextUnformatted("F1 toggles mouse capture");

        glm::vec3 camPos    = camera.position();
        float     yawDeg    = glm::degrees(camera.yaw());
        float     pitchDeg  = glm::degrees(camera.pitch());
        float     moveSpeed = camera.moveSpeed();
        float     mouseSens = camera.mouseSensitivity();
        float     fovDeg    = camera.fovDegrees();

        ImGui::DragFloat3("Position", glm::value_ptr(camPos), 0.05f);
        ImGui::DragFloat("Yaw (deg)", &yawDeg, 0.5f);
        ImGui::DragFloat("Pitch (deg)", &pitchDeg, 0.5f);
        ImGui::SliderFloat("Move speed", &moveSpeed, 0.5f, 60.0f, "%.2f");
        ImGui::SliderFloat("Mouse sensitivity", &mouseSens, 0.0001f, 0.02f, "%.4f");
        ImGui::SliderFloat("FOV (deg)", &fovDeg, 20.0f, 120.0f, "%.1f");

        camera.setPosition(camPos);
        camera.setYawPitch(glm::radians(yawDeg), glm::radians(pitchDeg));
        camera.setMoveSpeed(moveSpeed);
        camera.setMouseSensitivity(mouseSens);
        camera.setFovDegrees(fovDeg);

        ImGui::End();

        // --- Render -------------------------------------------------------
        gl::Viewport(0, 0, window.width(), window.height());
        gl::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
        gl::Clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

        shader.use();
        shader.setMat4("uProjection", camera.projectionMatrix());
        shader.setMat4("uView", camera.viewMatrix());
        shader.setVec3("uLightDir", glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f)));
        shader.setVec3("uLightColor", glm::vec3(1.0f, 0.98f, 0.92f));
        shader.setVec3("uViewPos", camera.position());

        // Ground.
        shader.setMat4("uModel", glm::mat4(1.0f));
        ground.draw();

        // A rotating gallery of shapes (cube, sphere, cylinder, cone, torus).
        const glm::vec3 positions[5] = {
            glm::vec3(-4.0f, 1.0f, -2.0f),
            glm::vec3(-2.0f, 1.0f, -2.0f),
            glm::vec3( 0.0f, 1.0f, -2.0f),
            glm::vec3( 2.0f, 1.0f, -2.0f),
            glm::vec3( 4.0f, 1.0f, -2.0f),
        };
        Mesh* shapes[5] = { &cube, &sphere, &cylinder, &cone, &torus };
        for (int i = 0; i < 5; ++i) {
            float spin = elapsed * (0.5f + 0.2f * i);
            glm::mat4 model = glm::translate(glm::mat4(1.0f), positions[i])
                            * glm::rotate(glm::mat4(1.0f), spin, glm::vec3(0.0f, 1.0f, 0.0f))
                            * glm::rotate(glm::mat4(1.0f), std::sin(elapsed + i) * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
            shader.setMat4("uModel", model);
            shapes[i]->draw();
        }

        // Draw the ImGui UI on top of the 3D scene.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        window.swap();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
