#include <cmath>
#include <memory>
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
#include "Window.h"
#include "core/Types.h"
#include "renderer/Renderer.h"
#include "scene/Scene.h"
#include "scene/World.h"

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

// ---------------------------------------------------------------------------
// Procedural geometry (unchanged from the original demo).
// ---------------------------------------------------------------------------

void pushFace(std::vector<Vertex>& out, const glm::vec3& n, const glm::vec3& color)
{
    // Unit cube, so each face is a 1x1 quad offset half a unit along its
    // outward normal. Without this offset every face passes through the
    // origin and the "cube" degenerates into six intersecting planes.
    constexpr float half = 0.5f;

    glm::vec3 ref = (std::fabs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                            : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 t = glm::normalize(glm::cross(n, ref));
    glm::vec3 b = glm::cross(n, t);

    // Centre of this face, sitting on the cube surface.
    glm::vec3 center = n * half;

    glm::vec3 c0 = center + (-t - b) * half;
    glm::vec3 c1 = center + ( t - b) * half;
    glm::vec3 c2 = center + ( t + b) * half;
    glm::vec3 c3 = center + (-t + b) * half;

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

std::vector<Vertex> makeCube()
{
    std::vector<Vertex> verts;
    pushFace(verts, glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.90f, 0.25f, 0.25f));
    pushFace(verts, glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.90f, 0.55f, 0.20f));
    pushFace(verts, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.20f, 0.80f, 0.25f));
    pushFace(verts, glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.25f, 0.35f, 0.90f));
    pushFace(verts, glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.90f, 0.90f, 0.25f));
    pushFace(verts, glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.70f, 0.25f, 0.80f));
    return verts;
}

void addTriangle(std::vector<Vertex>& out,
                 const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                 const glm::vec3& na, const glm::vec3& nb, const glm::vec3& nc,
                 const glm::vec3& color)
{
    out.push_back(Vertex{a, na, color});
    out.push_back(Vertex{b, nb, color});
    out.push_back(Vertex{c, nc, color});
}

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

std::vector<Vertex> makeCone(int sectors, float radius, float height, const glm::vec3& color)
{
    std::vector<Vertex> verts;
    const float halfH = height * 0.5f;
    const float m = radius / height;
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

// ---------------------------------------------------------------------------
// Scene building helpers
// ---------------------------------------------------------------------------

Entity addMeshEntity(Scene& scene, MeshHandle mesh, MaterialHandle material,
                     const glm::vec3& position, const glm::vec3& scale)
{
    Entity e = scene.create();

    Transform* t = scene.transforms().get(e);
    if (t) { t->position = position; t->scale = scale; }

    MeshRenderer& mr = scene.meshes().add(e, MeshRenderer{});
    mr.mesh     = mesh;
    mr.material = material;
    return e;
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

    // --- Renderer ---------------------------------------------------------
    // Shaders are copied next to the executable by CMake.
    std::string base = SDL_GetBasePath();
    Renderer renderer;
    if (!renderer.init(base)) {
        SDL_Log("Renderer init failed (shader load).");
        return 1;
    }

    // --- GPU resources ----------------------------------------------------
    MeshHandle groundMesh  = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeGround(24, 1.0f, -0.5f))));
    MeshHandle cubeMesh    = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeCube())));
    MeshHandle sphereMesh  = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeSphere(24, 32, 1.0f, glm::vec3(0.25f, 0.55f, 0.90f)))));
    MeshHandle cylMesh     = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeCylinder(32, 0.6f, 0.6f, glm::vec3(0.90f, 0.45f, 0.20f)))));
    MeshHandle coneMesh    = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeCone(32, 0.7f, 1.4f, glm::vec3(0.20f, 0.80f, 0.45f)))));
    MeshHandle torusMesh   = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeTorus(32, 24, 0.5f, 0.18f, glm::vec3(0.90f, 0.30f, 0.60f)))));

    Material groundMat;
    groundMat.specPower    = 16.0f;
    groundMat.specStrength = 0.05f;

    Material shapeMat;
    shapeMat.specPower    = 48.0f;
    shapeMat.specStrength = 0.40f;

    MaterialHandle groundMaterial = renderer.materials().add(groundMat);
    MaterialHandle shapeMaterial  = renderer.materials().add(shapeMat);

    // --- Scene ------------------------------------------------------------
    World world;
    Scene& scene = world.activeScene();

    addMeshEntity(scene, groundMesh, groundMaterial,
                  glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

    const glm::vec3 shapePositions[5] = {
        glm::vec3(-4.0f, 1.0f, -2.0f),
        glm::vec3(-2.0f, 1.0f, -2.0f),
        glm::vec3( 0.0f, 1.0f, -2.0f),
        glm::vec3( 2.0f, 1.0f, -2.0f),
        glm::vec3( 4.0f, 1.0f, -2.0f),
    };
    const MeshHandle shapeMeshes[5] = { cubeMesh, sphereMesh, cylMesh, coneMesh, torusMesh };
    Entity shapeEntities[5];
    for (int i = 0; i < 5; ++i) {
        shapeEntities[i] = addMeshEntity(scene, shapeMeshes[i], shapeMaterial,
                                         shapePositions[i], glm::vec3(1.0f));
    }

    // Directional light with shadows enabled.
    Entity sunEntity = scene.create();
    {
        Transform* t = scene.transforms().get(sunEntity);
        if (t) t->rotation = glm::quat(glm::vec3(glm::radians(-50.0f),
                                                 glm::radians(-35.0f), 0.0f));
        LightComponent& lc = scene.lights().add(sunEntity, LightComponent{});
        lc.type        = LightType::Directional;
        lc.color       = glm::vec3(1.0f, 0.97f, 0.90f);
        lc.intensity   = 1.0f;
        lc.castsShadow = true;
    }

    // Two dynamic point lights WITHOUT shadows.
    Entity lightAEntity = scene.create();
    {
        LightComponent& lc = scene.lights().add(lightAEntity, LightComponent{});
        lc.type        = LightType::Point;
        lc.color       = glm::vec3(1.0f, 0.35f, 0.20f);
        lc.intensity   = 3.0f;
        lc.range       = 7.0f;
        lc.castsShadow = false;
    }
    Entity lightBEntity = scene.create();
    {
        LightComponent& lc = scene.lights().add(lightBEntity, LightComponent{});
        lc.type        = LightType::Point;
        lc.color       = glm::vec3(0.25f, 0.55f, 1.0f);
        lc.intensity   = 3.0f;
        lc.range       = 7.0f;
        lc.castsShadow = false;
    }

    // Camera entity (the CameraComponent owns the camera state).
    Entity cameraEntity = scene.create();
    scene.cameras().add(cameraEntity, CameraComponent{});

    // Sun direction controls (degrees), applied to the sun transform each frame.
    float sunPitchDeg = -50.0f;
    float sunYawDeg   = -35.0f;

    window.setRelativeMouseMode(true);

    Uint64 last = SDL_GetTicks64();
    float elapsed = 0.0f;
    bool wireframe = false;
    bool wasF = false;
    bool wasF1 = false;

    float fpsSmooth = 0.0f;
    float fpsHistory[120] = {0};
    int   fpsHistoryCount = 0;

    while (window.pollEvents()) {
        Uint64 now = SDL_GetTicks64();
        float dt = static_cast<float>(now - last) / 1000.0f;
        last = now;
        if (dt > 0.25f) dt = 0.25f;
        elapsed += dt;

        CameraComponent* camComp = scene.cameras().get(cameraEntity);
        Camera& camera = camComp->camera;

        // Toggle wireframe with F (rising-edge detection).
        bool isF = window.keyDown(SDL_SCANCODE_F);
        if (isF && !wasF) {
            wireframe = !wireframe;
            gl::PolygonMode(gl::FRONT_AND_BACK, wireframe ? gl::LINE : gl::FILL);
        }
        wasF = isF;

        // F1 toggles mouse capture.
        bool isF1 = window.keyDown(SDL_SCANCODE_F1);
        if (isF1 && !wasF1) {
            g_cursorCaptured = !g_cursorCaptured;
            window.setRelativeMouseMode(g_cursorCaptured);
        }
        wasF1 = isF1;

        // --- ImGui frame --------------------------------------------------
        ImGuiIO& io = ImGui::GetIO();
        if (g_cursorCaptured) io.ConfigFlags |=  ImGuiConfigFlags_NoMouse;
        else                  io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (g_cursorCaptured) suppressImGuiMouse(io);

        // --- Camera input -------------------------------------------------
        camera.setAspect(static_cast<float>(window.width()) / window.height());

        float fwd = 0.0f, rgt = 0.0f, up = 0.0f;
        if (!io.WantCaptureKeyboard) {
            if (window.keyDown(SDL_SCANCODE_W) || window.keyDown(SDL_SCANCODE_UP))    fwd += 1.0f;
            if (window.keyDown(SDL_SCANCODE_S) || window.keyDown(SDL_SCANCODE_DOWN))  fwd -= 1.0f;
            if (window.keyDown(SDL_SCANCODE_D) || window.keyDown(SDL_SCANCODE_RIGHT)) rgt += 1.0f;
            if (window.keyDown(SDL_SCANCODE_A) || window.keyDown(SDL_SCANCODE_LEFT))  rgt -= 1.0f;
            if (window.keyDown(SDL_SCANCODE_SPACE)) up += 1.0f;
            if (window.keyDown(SDL_SCANCODE_LCTRL) || window.keyDown(SDL_SCANCODE_C)) up -= 1.0f;
            camera.processKeyboard(fwd, rgt, up, dt);
        }

        float mdx = 0.0f, mdy = 0.0f;
        window.mouseDelta(mdx, mdy);
        if (g_cursorCaptured && (mdx != 0.0f || mdy != 0.0f)) camera.processMouse(mdx, mdy);

        // --- Animate the scene --------------------------------------------
        for (int i = 0; i < 5; ++i) {
            Transform* t = scene.transforms().get(shapeEntities[i]);
            if (!t) continue;
            const float spin = elapsed * (0.5f + 0.2f * i);
            const glm::quat yaw   = glm::quat(glm::vec3(0.0f, spin, 0.0f));
            const glm::quat tilt  = glm::quat(glm::vec3(std::sin(elapsed + i) * 0.5f, 0.0f, 0.0f));
            t->rotation = yaw * tilt;
        }

        // The two point lights orbit the shape gallery (dynamic lights).
        {
            Transform* ta = scene.transforms().get(lightAEntity);
            if (ta) {
                const float a = elapsed * 1.1f;
                ta->position = glm::vec3(std::cos(a) * 3.5f, 1.8f, -2.0f + std::sin(a) * 3.0f);
            }
            Transform* tb = scene.transforms().get(lightBEntity);
            if (tb) {
                const float b = elapsed * 1.1f + PI;
                tb->position = glm::vec3(std::cos(b) * 3.5f, 2.4f, -2.0f + std::sin(b) * 3.0f);
            }
        }

        Transform* sunT = scene.transforms().get(sunEntity);
        if (sunT) {
            sunT->rotation = glm::quat(glm::vec3(glm::radians(sunPitchDeg),
                                                 glm::radians(sunYawDeg), 0.0f));
        }

        // --- ImGui UI -----------------------------------------------------
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

        ImGui::SetNextWindowPos(ImVec2(430, 110), ImGuiCond_FirstUseEver);
        ImGui::Begin("Lighting");

        LightComponent* sun = scene.lights().get(sunEntity);
        if (sun) {
            ImGui::TextUnformatted("Directional light (shadow-casting)");
            ImGui::ColorEdit3("Sun color", glm::value_ptr(sun->color));
            ImGui::SliderFloat("Sun intensity", &sun->intensity, 0.0f, 3.0f, "%.2f");
            ImGui::Checkbox("Casts shadow", &sun->castsShadow);
            ImGui::SliderFloat("Sun pitch", &sunPitchDeg, -89.0f, -5.0f, "%.0f deg");
            ImGui::SliderFloat("Sun yaw",   &sunYawDeg,  -180.0f, 180.0f, "%.0f deg");
        }
        ImGui::Separator();

        LightComponent* la = scene.lights().get(lightAEntity);
        Transform*      ta = scene.transforms().get(lightAEntity);
        if (la) {
            ImGui::TextUnformatted("Point light A (no shadow)");
            ImGui::ColorEdit3("A color", glm::value_ptr(la->color));
            ImGui::SliderFloat("A intensity", &la->intensity, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("A range", &la->range, 1.0f, 20.0f, "%.2f");
            if (ta) ImGui::Text("A pos: %.2f, %.2f, %.2f", ta->position.x, ta->position.y, ta->position.z);
        }
        ImGui::Separator();

        LightComponent* lb = scene.lights().get(lightBEntity);
        Transform*      tb = scene.transforms().get(lightBEntity);
        if (lb) {
            ImGui::TextUnformatted("Point light B (no shadow)");
            ImGui::ColorEdit3("B color", glm::value_ptr(lb->color));
            ImGui::SliderFloat("B intensity", &lb->intensity, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("B range", &lb->range, 1.0f, 20.0f, "%.2f");
            if (tb) ImGui::Text("B pos: %.2f, %.2f, %.2f", tb->position.x, tb->position.y, tb->position.z);
        }
        ImGui::End();

        ImGui::Render();

        // --- Update + render through the renderer --------------------------
        world.update(dt);

        RenderView view;
        view.scene     = &scene;
        view.camera    = camera;
        view.viewport  = Viewport{ 0, 0, window.width(), window.height() };
        view.passes    = Pass_Default;
        view.layerMask = 1u;
        view.order     = 0;

        renderer.setOutputSize(window.width(), window.height());

        std::vector<RenderView> views;
        views.push_back(view);
        renderer.renderFrame(views, scene);

        window.swap();
    }

    renderer.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
