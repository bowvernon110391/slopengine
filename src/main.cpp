#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
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

// Ground plane height. Shared by makeGround() and prop placement so the two
// cannot drift apart -- seating props against a stale ground height would leave
// the whole grounded tier floating or half-buried.
constexpr float kGroundY = -0.5f;

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

// 'isStatic' freezes the transform and lets the renderer cache the object's
// world-space bounds across frames.
Entity addMeshEntity(Scene& scene, const MeshCache& meshes,
                     MeshHandle mesh, MaterialHandle material,
                     const glm::vec3& position, const glm::vec3& scale,
                     bool isStatic = false)
{
    Entity e = scene.create();

    Transform* t = scene.transforms().get(e);
    if (t) {
        t->position = position;
        t->scale    = scale;
        t->isStatic = isStatic;
    }

    MeshRenderer& mr = scene.meshes().add(e, MeshRenderer{});
    mr.mesh     = mesh;
    mr.material = material;
    if (isStatic) mr.flags |= kMeshFlagStatic;

    // Local-space bounds. The render queue transforms these into world space and
    // uses them for frustum culling, depth sorting and the debug overlay -- so
    // leaving them unset would silently disable all three.
    const Mesh* m = meshes.get(mesh);
    if (m) mr.worldBounds = m->bounds();

    return e;
}

// A handful of shared materials that props reuse, so the renderer's material
// batching has something to batch and the uniform uploads actually collapse.
std::vector<MaterialHandle> makeMaterialPalette(Renderer& renderer)
{
    const glm::vec3 albedos[6] = {
        glm::vec3(0.85f, 0.30f, 0.25f),
        glm::vec3(0.25f, 0.60f, 0.85f),
        glm::vec3(0.35f, 0.80f, 0.45f),
        glm::vec3(0.90f, 0.75f, 0.30f),
        glm::vec3(0.70f, 0.40f, 0.85f),
        glm::vec3(0.90f, 0.55f, 0.25f),
    };

    std::vector<MaterialHandle> palette;
    palette.reserve(6);
    for (int i = 0; i < 6; ++i) {
        Material m;
        m.albedo       = albedos[i];
        m.specPower    = 16.0f + 8.0f * static_cast<float>(i);
        m.specStrength = 0.15f + 0.06f * static_cast<float>(i);
        palette.push_back(renderer.materials().add(m));
    }
    return palette;
}

// An already-placed prop, used to keep later props from intersecting it.
//
// Each prop is bounded by a vertical cylinder: a circumscribed XZ radius plus a
// vertical half-extent. Two such cylinders are disjoint when they are separated
// along XZ *or* along Y, which is the test applied below.
//
// The XZ radius is the circumscribed one rather than the tight one. That is the
// honest bound for the dynamic props: they spin about Y, so they sweep their whole
// circumscribed circle.
struct Placement
{
    glm::vec2 xz;
    float     radius;   // circumscribed XZ radius
    float     y;        // centre, world space
    float     yHalf;    // half-extent along Y
};

// Scatters props over a disc centred on the origin. The radius is kept inside the
// ground plane (makeGround() spans +/-16), so nothing hangs over the edge of the
// world.
//
// Roughly half the props are seated on the ground and half float well above it, so
// the high ones throw shadows onto the ground and onto whatever sits beneath them.
// Roughly three quarters are static, which is what exercises the frozen-transform
// and cached-bounds paths.
//
// 'seed' is supplied by the caller so the same field can be regenerated, or a fresh
// one produced, without editing the code. 'outProps' receives every prop so the
// field can later be torn down; 'outDynamic' receives the subset that animates.
void spawnField(Scene& scene, const MeshCache& meshes,
                const MeshHandle* shapes, int shapeCount,
                const std::vector<MaterialHandle>& palette,
                std::uint32_t seed,
                std::vector<Entity>& outProps,
                std::vector<Entity>& outDynamic)
{
    constexpr float kFieldRadius = 13.0f;   // < half the ground extent (16)
    constexpr float kFloatYMin   = 3.0f;
    constexpr float kFloatYMax   = 14.0f;
    constexpr int   kPropCount   = 64;
    constexpr int   kPlaceTries  = 32;      // rejection attempts per prop
    constexpr float kPadding     = 1.15f;   // slack on each required clearance

    std::mt19937 rng(seed);   // caller-supplied: a new seed gives a new layout
    std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> floatHeightDist(kFloatYMin, kFloatYMax);
    std::uniform_real_distribution<float> scaleDist(0.5f, 1.6f);
    std::uniform_int_distribution<int>    shapeDist(0, shapeCount - 1);
    std::uniform_int_distribution<int>    materialDist(0, static_cast<int>(palette.size()) - 1);

    std::vector<Placement> placed;
    placed.reserve(kPropCount);

    for (int i = 0; i < kPropCount; ++i) {
        // Grounded / static are drawn independently rather than derived from the
        // loop index: branching on i % 2 and i % 4 would correlate the two, so
        // "grounded" would imply "more likely static" and the two populations
        // would stop looking independent.
        const bool grounded = unitDist(rng) < 0.5f;
        const bool isStatic = unitDist(rng) < 0.75f;

        const MeshHandle shape = shapes[shapeDist(rng)];
        const Mesh* mesh = meshes.get(shape);

        // Local bounds, so a prop can be seated on the ground without hardcoding
        // a half-height per shape.
        glm::vec3 half(0.5f);
        glm::vec3 localMin(-0.5f);
        if (mesh) {
            const AABB& b = mesh->bounds();
            half     = b.extents();
            localMin = b.min;
        }

        const float s     = scaleDist(rng);
        const float foot  = glm::length(glm::vec2(half.x, half.z)) * s;
        const float yHalf = half.y * s;

        // The verticality: either seated on the plane, or floating far enough above
        // it that the prop shades whatever ends up underneath it. Seating derives
        // from the mesh's own min.y rather than a fixed offset, so it stays correct
        // for a mesh that is not centred on its origin.
        const float y = grounded ? (kGroundY - localMin.y * s)   // base on the plane
                                 : floatHeightDist(rng);          // floating centre

        // Rejection sampling against everything already placed. This replaces a
        // lattice snap that was actively harmful: rounding positions to a 2-unit
        // grid dropped several props onto the same cell, and those coincident pairs
        // are what used to intersect.
        glm::vec2 bestXZ(0.0f);
        float bestClear = -1e9f;
        for (int attempt = 0; attempt < kPlaceTries; ++attempt) {
            // sqrt()-style remap of a uniform sample gives even area density over
            // the disc; the exponent softens that into a slight centre bias so the
            // field reads as a cluster rather than a uniform scatter.
            const float rad = kFieldRadius * std::pow(unitDist(rng), 0.55f);
            const float ang = angleDist(rng);
            const glm::vec2 xz(rad * std::cos(ang), rad * std::sin(ang));

            float clear = 1e9f;
            for (const Placement& p : placed) {
                // Two vertical cylinders intersect only if they overlap on BOTH
                // axes, so taking the larger of the two margins expresses exactly
                // "disjoint in XZ, or disjoint in Y". Requiring XZ clearance alone
                // would forbid floating a prop above a grounded one, which is the
                // entire point of the height split.
                const float dxz = glm::length(xz - p.xz) - (foot + p.radius) * kPadding;
                const float dy  = std::abs(y - p.y) - (yHalf + p.yHalf);
                clear = std::min(clear, std::max(dxz, dy));
            }
            if (clear >= 0.0f) { bestXZ = xz; break; }

            // Nothing fits: keep the roomiest candidate seen, so the prop count
            // stays exact and the loop always terminates.
            if (clear > bestClear) { bestClear = clear; bestXZ = xz; }
        }
        placed.push_back(Placement{ bestXZ, foot, y, yHalf });

        Entity e = addMeshEntity(scene, meshes, shape,
                                 palette[materialDist(rng)],
                                 glm::vec3(bestXZ.x, y, bestXZ.y),
                                 glm::vec3(s),
                                 isStatic);

        // Every prop is reported, not just the animated ones: respawning has to be
        // able to tear the whole field down, and the static majority is otherwise
        // unreachable.
        outProps.push_back(e);

        if (!isStatic) {
            Transform* t = scene.transforms().get(e);
            if (t) {
                t->rotation = glm::quat(glm::vec3(angleDist(rng) * 0.4f,
                                                  angleDist(rng), 0.0f));
            }
            outDynamic.push_back(e);
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    Window window("Slop Engine v0.1 alpha", 800, 600);
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
    MeshHandle groundMesh  = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeGround(32, 1.0f, kGroundY))));
    MeshHandle cubeMesh    = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeCube())));
    MeshHandle sphereMesh  = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeSphere(24, 32, 1.0f, glm::vec3(0.25f, 0.55f, 0.90f)))));
    MeshHandle cylMesh     = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeCylinder(32, 0.6f, 0.6f, glm::vec3(0.90f, 0.45f, 0.20f)))));
    MeshHandle coneMesh    = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeCone(32, 0.7f, 1.4f, glm::vec3(0.20f, 0.80f, 0.45f)))));
    MeshHandle torusMesh   = renderer.meshes().add(std::unique_ptr<Mesh>(new Mesh(makeTorus(32, 24, 0.5f, 0.18f, glm::vec3(0.90f, 0.30f, 0.60f)))));

    Material groundMat;
    groundMat.specPower    = 16.0f;
    groundMat.specStrength = 0.05f;

    MaterialHandle groundMaterial = renderer.materials().add(groundMat);

    // --- Scene ------------------------------------------------------------
    World world;
    Scene& scene = world.activeScene();

    addMeshEntity(scene, renderer.meshes(), groundMesh, groundMaterial,
                  glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f), true);

    // --- Random prop field ------------------------------------------------
    // The ground is the only object placed by hand. Everything else comes from the
    // field below, so all five shape meshes reach the scene through the spawner and
    // the scene is laid out entirely at random.
    const MeshHandle shapeMeshes[5] = { cubeMesh, sphereMesh, cylMesh, coneMesh, torusMesh };

    // A wider spread of objects sharing a small material palette: this is what
    // exercises frustum culling, the static/dynamic split and material batching.
    const std::vector<MaterialHandle> palette = makeMaterialPalette(renderer);

    // A fresh seed per launch, so the layout actually differs between runs. The
    // "Re-randomise" button on the Performance tab draws a new one on demand, and
    // the value is displayed so a layout can still be reported and reproduced.
    //
    // NOTE: std::random_device is non-deterministic on MSVC, which is what this
    // project builds with. On some MinGW/libstdc++ builds it is deterministic, so
    // if the layout ever becomes fixed again, mix in SDL_GetPerformanceCounter().
    std::uint32_t placementSeed = std::random_device{}();

    std::vector<Entity> propEntities;    // every prop, static and dynamic
    std::vector<Entity> dynamicProps;    // the animated subset

    // One code path for the initial spawn and for re-randomising, so the two cannot
    // drift apart.
    auto spawnProps = [&]() {
        for (const Entity e : propEntities) {
            if (scene.alive(e)) scene.destroy(e);
        }
        // Rebuilt from scratch, never appended to: destroying frees entity ids and
        // create() reuses them, so a retained handle would alias a brand-new prop.
        propEntities.clear();
        dynamicProps.clear();
        spawnField(scene, renderer.meshes(), shapeMeshes, 5, palette,
                   placementSeed, propEntities, dynamicProps);
    };
    spawnProps();

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

    // Debug/measurement state.
    bool        cullEnabled = true;
    bool        showAABBs   = false;
    std::size_t statVisible = 0;
    std::size_t statCulled  = 0;
    std::size_t statDraws   = 0;

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
        // Only the dynamic props move; the static ones are frozen, which is what
        // lets updateTransforms() skip their whole subtree.
        for (std::size_t i = 0; i < dynamicProps.size(); ++i) {
            Transform* t = scene.transforms().get(dynamicProps[i]);
            if (!t) continue;
            const float fi   = static_cast<float>(i);
            const float spin = elapsed * (0.4f + 0.15f * static_cast<float>(i % 5));
            const glm::quat yaw  = glm::quat(glm::vec3(0.0f, spin, 0.0f));
            const glm::quat tilt = glm::quat(glm::vec3(std::sin(elapsed + fi) * 0.4f, 0.0f, 0.0f));
            t->rotation = yaw * tilt;
        }

        // The two point lights orbit the centre of the field (dynamic lights).
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
        ImGui::SetNextWindowSize(ImVec2(500, 560), ImGuiCond_FirstUseEver);
        ImGui::Begin("Slop Engine");

        // Sliders default to ~65% of the content width, which starves the label
        // column and clips longer captions ("Poisson radius (texels)"). A fixed item
        // width keeps every control consistent and leaves room for the text.
        ImGui::PushItemWidth(240.0f);

        // One window, one tab per former panel. Fixed size rather than auto-resize:
        // the tabs differ a lot in height (Shadow Maps carries a 256x256 preview),
        // so sizing to content would make the window jump on every switch.
        //
        // Note ImGui submits widgets for the ACTIVE tab only, so the per-frame setter
        // calls inside a hidden panel do not run. That is harmless here: each value is
        // only changed by a widget in its own panel and the destination keeps its last
        // value, so a hidden tab simply leaves its settings as they were.
        if (ImGui::BeginTabBar("##panels", ImGuiTabBarFlags_Reorderable)) {

        if (ImGui::BeginTabItem("Performance")) {
        ImGui::Text("FPS: %.1f", fpsSmooth);
        ImGui::Text("Frame time: %.2f ms", dt * 1000.0f);
        int n = fpsHistoryCount < 120 ? fpsHistoryCount : 120;
        ImGui::PlotLines("##fps", fpsHistory, n, 0, nullptr, 0.0f, 240.0f, ImVec2(0, 50));

        ImGui::Separator();
        const std::size_t objectCount = scene.meshes().size();
        std::size_t staticCount = 0;
        for (const Transform& t : scene.transforms().dense()) {
            if (t.isStatic) ++staticCount;
        }
        ImGui::Text("Objects: %d (static %d / dynamic %d)",
                    static_cast<int>(objectCount),
                    static_cast<int>(staticCount),
                    static_cast<int>(objectCount - staticCount));
        ImGui::Text("Visible: %d  Culled: %d",
                    static_cast<int>(statVisible), static_cast<int>(statCulled));
        ImGui::Text("Draw calls: %d", static_cast<int>(statDraws));

        // Rebuilds the whole prop field from a new seed. Placed below the stats
        // above so a mid-frame destroy cannot leave those numbers describing the
        // previous field. The respawn happens before this frame's world.update()
        // and renderFrame(), so transforms, bounds and the draw lists all pick it up
        // within the same iteration.
        if (ImGui::Button("Re-randomise placement")) {
            placementSeed = std::random_device{}();
            spawnProps();
        }
        ImGui::SameLine();
        ImGui::Text("seed %u", static_cast<unsigned>(placementSeed));

        ImGui::Checkbox("Frustum culling", &cullEnabled);
        ImGui::Checkbox("Show AABBs", &showAABBs);
        renderer.setDebugAABBs(showAABBs);
        ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Camera")) {
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
        ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Lighting")) {
        ImGui::TextUnformatted("Scene ambient (flat, unshadowed)");
        ImGui::ColorEdit3("Ambient", glm::value_ptr(scene.ambient));
        ImGui::Separator();

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
        ImGui::EndTabItem();
        }

        // --- Shadow map debug ---------------------------------------------
        // Built before renderFrame(), so the images shown are from the previous
        // frame's shadow pass.
        if (ImGui::BeginTabItem("Shadow Maps")) {

        bool shadowDebug = renderer.shadowDebugEnabled();
        if (ImGui::Checkbox("Enable preview", &shadowDebug)) {
            renderer.setShadowDebugEnabled(shadowDebug);
        }

        float depthMin   = renderer.shadowDebugMin();
        float depthMax   = renderer.shadowDebugMax();
        bool  invertView = renderer.shadowDebugInvert();
        ImGui::SliderFloat("Min depth", &depthMin, 0.0f, 1.0f, "%.4f");
        ImGui::SliderFloat("Max depth", &depthMax, 0.0f, 1.0f, "%.4f");
        ImGui::Checkbox("Invert", &invertView);
        if (depthMax <= depthMin) depthMin = depthMax - 0.001f;
        renderer.setShadowDebugRange(depthMin, depthMax, invertView);

        const std::vector<ShadowMapView>& shadowViews = renderer.shadowMapViews();
        ImGui::Separator();
        ImGui::Text("Shadow maps: %d", static_cast<int>(shadowViews.size()));

        // --- Shadow fit ----------------------------------------------------
        // The ortho follows the camera, so these directly trade distant shadows
        // for near-camera sharpness.
        ImGui::Separator();
        ImGui::TextUnformatted("Shadow fit");
        ShadowFitParams fit = renderer.shadowFit();
        ImGui::SliderFloat("Distance", &fit.shadowDistance, 10.0f, 120.0f, "%.0f");
        ImGui::SliderFloat("Fade fraction", &fit.fadeFraction, 0.0f, 0.5f, "%.2f");
        ImGui::SliderFloat("Max extrusion", &fit.maxExtrusion, 0.0f, 200.0f, "%.0f");
        ImGui::Checkbox("Extrude for casters", &fit.extrudeForCasters);
        ImGui::Checkbox("Texel snap", &fit.texelSnap);
        renderer.setShadowFit(fit);

        // --- Shadow filtering ----------------------------------------------
        // The width of the Poisson disc the shadow lookup samples with. Separate
        // from the fit above: this filters the sampled result, it does not change
        // how the light's ortho is fitted. Widening it softens the penumbra at a
        // fixed 16 taps, but more lit taps get averaged in near a contact, so a
        // radius that is too large shrinks the shadow there.
        ImGui::Separator();
        float pcfRadius = renderer.forwardPass().pcfRadius();
        if (ImGui::SliderFloat("Poisson radius (texels)", &pcfRadius, 0.5f, 6.0f, "%.2f")) {
            renderer.forwardPass().setPcfRadius(pcfRadius);
        }

        if (shadowViews.empty()) {
            ImGui::TextUnformatted(shadowDebug
                ? "None (no light casts a shadow)"
                : "Enable the preview to inspect the maps");
        }

        for (std::size_t i = 0; i < shadowViews.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("Map %d  (%d x %d)", static_cast<int>(i),
                        shadowViews[i].sourceSize, shadowViews[i].sourceSize);
            if (shadowViews[i].texture != 0) {
                ImGui::Image((ImTextureID)shadowViews[i].texture, ImVec2(256, 256));
            } else {
                ImGui::TextUnformatted("No preview available");
            }
            ImGui::PopID();
        }
        ImGui::EndTabItem();
        }

        // --- Shadow bias ----------------------------------------------------
        // The depth bias the shadow lookup compares with. It is the only guard
        // against acne now that polygon offset is off, and these three terms trade
        // directly against each other (see slopeScaledBias() in common/lighting.glsl).
        //
        // Note the multiplication in slopeScaledBias(): with the base bias at 0
        // the whole expression is 0 and the Max bias cap below is inert, so Slope
        // scale and Max bias do nothing until Base bias is raised.
        if (ImGui::BeginTabItem("Shadow Bias")) {

        float bias      = renderer.forwardPass().shadowBias();
        float slopeBias = renderer.forwardPass().shadowSlopeBias();
        float maxBias   = renderer.forwardPass().shadowMaxBias();

        // Five decimals for the two absolute biases: they are of order 1e-3, so a
        // coarser format would show every value as "0.00" and be useless.
        ImGui::SliderFloat("Base bias", &bias, 0.0f, 0.010f, "%.5f");
        ImGui::SliderFloat("Slope scale", &slopeBias, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Max bias", &maxBias, 0.0f, 0.050f, "%.5f");

        renderer.forwardPass().setShadowBias(bias);
        renderer.forwardPass().setShadowSlopeBias(slopeBias);
        renderer.forwardPass().setShadowMaxBias(maxBias);

        if (ImGui::Button("Reset")) {
            renderer.forwardPass().resetShadowBias();
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Lower bias keeps shadows attached to the object casting them but lets "
            "acne through; higher bias does the reverse. Polygon offset is disabled, "
            "so these are the only guard against acne.");
        ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        }

        ImGui::PopItemWidth();
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
        view.queue.setCullingEnabled(cullEnabled);

        renderer.setOutputSize(window.width(), window.height());

        std::vector<RenderView> views;
        views.push_back(view);
        renderer.renderFrame(views, scene);

        statVisible = renderer.statVisible();
        statCulled  = renderer.statCulled();
        statDraws   = renderer.statDraws();

        window.swap();
    }

    renderer.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
