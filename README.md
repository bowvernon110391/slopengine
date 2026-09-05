# SDL2 + OpenGL 3D Demo

A minimal, self-contained C++ project that:

1. Creates a window and handles input with **SDL2**,
2. Renders a 3D scene on top of that window with **OpenGL 3.3 Core Profile**,
3. Is written in **C++11**,
4. Builds with **CMake** (SDL2 and GLM are downloaded and built automatically — no manual install).

The scene contains a lit checkerboard ground plane and a rotating gallery of
procedurally generated 3D shapes (cube, sphere, cylinder, cone, and torus),
viewed through a first-person camera you can fly around with mouse + keyboard.

---

## Requirements

| Tool | Version | Notes |
|------|---------|-------|
| CMake | 3.16+ | |
| A C/C++ compiler | MSVC 2019/2022, GCC, or Clang | On Windows, Visual Studio 2022 Community is used here |
| Git | any recent | Used only to download SDL2 and GLM once |
| Internet access | — | Needed on the first configure to fetch SDL2 and GLM |

Everything else (SDL2 and GLM) is fetched and built automatically by CMake via
`FetchContent`, so there is nothing else to install. The OpenGL loader is
hand-written (see `src/GL.h`), so no GLEW/GLAD/gl3w dependency is required.

---

## Building

Open a terminal in the project root and run:

```bash
# 1) Configure (downloads + configures SDL2 and GLM on first run)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# 2) Build
cmake --build build --config Release
```

> **Other generators work too**, e.g. on Windows with MinGW:
> `cmake -S . -B build -G "MinGW Makefiles"`, or on Linux/macOS just
> `cmake -S . -B build && cmake --build build`.

The resulting executable is:

- Windows (MSVC): `build/Release/sdl2_opengl_demo.exe`
- Linux/macOS: `build/sdl2_opengl_demo`

CMake copies the `shaders/` folder next to the executable automatically, so the
binary finds its shaders regardless of your working directory.

### Run

```bash
# Windows (MSVC)
build\Release\sdl2_opengl_demo.exe

# Linux / macOS
./build/sdl2_opengl_demo
```

---

## Controls

| Key(s) | Action |
|--------|--------|
| `W` / `S` (or arrows) | Move forward / backward |
| `A` / `D` (or arrows) | Strafe left / right |
| `Space` / `Ctrl` | Move up / down |
| Mouse | Look around |
| `F` | Toggle wireframe |
| `Esc` | Quit |

The mouse is captured (`SDL_SetRelativeMouseMode`) while the app runs.

---

## Project structure

```
.
├── CMakeLists.txt          # Build configuration + SDL2/GLM fetching
├── README.md
├── .gitignore
├── shaders/
│   ├── basic.glsl          # Combined vertex + fragment shader (single file)
│   └── common/
│       └── lighting.glsl   # Reusable lighting function (#include'd by basic.glsl)
└── src/
    ├── main.cpp            # Entry point + game loop + scene setup
    ├── Window.h / .cpp     # SDL window + OpenGL context (RAII)
    ├── GL.h / .cpp         # Minimal OpenGL 3.3 core loader (no GLEW/GLAD)
    ├── Camera.h / .cpp     # First-person camera (mouse look + WASD)
    ├── Shader.h / .cpp     # Shader preprocessor (#include/#define) + compile/link + uniforms
    └── Mesh.h / .cpp       # VAO/VBO/EBO wrapper for a mesh
```

---

## How it works

### Window & input (`Window`)
`Window` initializes SDL's video subsystem, requests an **OpenGL 3.3 Core**
context with `SDL_GL_SetAttribute`, creates the window and context, and enables
vsync. `pollEvents()` drains the SDL event queue, tracking window resizes,
relative mouse motion, and keyboard state (`SDL_GetKeyboardState`).

### OpenGL loading (`GL`)
Windows only exports OpenGL 1.1 functions from `opengl32.dll`. Everything newer
(VAOs, shaders, etc.) must be fetched at runtime. Instead of pulling in a loader
library, `GL.h`/`GL.cpp` declare the exact subset of the GL 3.3 API the demo uses
and resolve each function pointer with `SDL_GL_GetProcAddress`. All symbols live
in a `gl` namespace to avoid clashing with any real GL headers.

### Rendering
- **Shaders** (`shaders/`) do a standard MVP transform plus a single directional
  light (ambient + diffuse + specular).
- **Meshes** (`Mesh`) upload interleaved `position/normal/color` data to a VAO
  and draw with `glDrawArrays` (non-indexed) or `glDrawElements`.
- **Camera** (`Camera`) builds a perspective + look-at view matrix, and handles
  mouse-look (yaw/pitch) and WASD movement.
- **Math** is provided by **GLM** (header-only), which supplies `vec3`/`mat4`
  plus `perspective`, `lookAt`, `translate`, `rotate`, `normalize`, `dot`,
  `cross`, and the other vector/quaternion/matrix operations the demo needs.

### Shader files

Each shader is a single `.glsl` file that contains **both** stages, separated by
markers:

```glsl
#type vertex
#version 330 core
// ... vertex stage ...

#type fragment
#version 330 core
#include "common/lighting.glsl"
// ... fragment stage ...
```

- `#type vertex` / `#type fragment` separate the two stages (everything before
  the first marker is ignored, so file headers/comments are fine).
- `#include "path"` (or `#include <path>`) pulls in another shader file, resolved
  relative to the including file and expanded **recursively** (cycles are
  detected and reported). This is how reusable functions like
  `common/lighting.glsl` are shared between shaders.
- `#define` flags in the source are reported to C++ via `Shader::defines()` /
  `Shader::hasDefine()`, and extra flags can be injected with
  `Shader::addDefine("FLAG", "1")`.

### The scene
A 24×24 checkerboard ground plane is generated in code, and a gallery of five
procedurally generated shapes — a cube, UV-sphere, cylinder, cone, and torus —
spin in place above it. All geometry (including smooth normals) is built
procedurally in `main.cpp` (see `makeCube`, `makeSphere`, `makeCylinder`,
`makeCone`, `makeTorus`). Depth testing and back-face culling are enabled;
`F` toggles wireframe mode.

---

## Troubleshooting

- **"Failed to create window / OpenGL context"** — your GPU/driver may not
  support OpenGL 3.3 core, or you're on a remote/software GL context. Try a
  machine with a modern driver.
- **"Failed to load OpenGL function: ..."** — means the context doesn't expose a
  needed 3.3 entry point (same cause as above).
- **"Shader file not found or empty"** — the `shaders/` folder wasn't copied next
  to the executable. Rebuild so the `POST_BUILD` copy step runs.
- **CMake can't find SDL2/GLM / download fails** — check your internet connection
  and Git. The project downloads `libsdl-org/SDL` at tag `release-2.30.12` and
  `g-truc/glm` at tag `1.0.1`.

---

## Notes / extensions

- SDL2 is built **statically** (`SDL_STATIC=ON`), so the demo is a single
  self-contained executable with no `SDL2.dll` to deploy.
- To change the SDL2 or GLM version, edit the corresponding `GIT_TAG` in
  `CMakeLists.txt`.
- Want textures, a skybox, or more models? Extend `Mesh`/`Shader` and add more
  GLSL uniforms — the loader in `GL.h` only declares what's currently used, so
  add new functions there if you use additional GL calls.
