cmake_minimum_required(VERSION 3.16)

project(SDL2OpenGLDemo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# SDL2 dependency (fetched automatically - no manual install required).
# ---------------------------------------------------------------------------
include(FetchContent)

# Build SDL2 as a static library so the demo produces a single self-contained
# executable (no SDL2.dll to copy around).
set(SDL_SHARED  OFF CACHE BOOL "" FORCE)
set(SDL_STATIC  ON  CACHE BOOL "" FORCE)
set(SDL_TESTS   OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2)

# ---------------------------------------------------------------------------
# GLM dependency (header-only math: vector / quaternion / matrix operations).
# ---------------------------------------------------------------------------
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)  # use it purely header-only
set(GLM_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ---------------------------------------------------------------------------
# ImGui dependency (immediate-mode GUI, fetched automatically).
#
# Dear ImGui has no CMake target in this version (the repository ships only
# sources + backends), so we compile the core files directly into the
# executable in add_executable() below.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(sdl2_opengl_demo WIN32
    src/main.cpp
    src/GL.cpp
    src/Shader.cpp
    src/Mesh.cpp
    src/Camera.cpp
    src/Window.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(sdl2_opengl_demo PRIVATE
    src
    ${imgui_SOURCE_DIR}          # imgui.h
    ${imgui_SOURCE_DIR}/backends # imgui_impl_*.h
)

# SDL2::SDL2     - the SDL2 library
# SDL2::SDL2main - platform "main()" glue (WinMain on Windows, etc.)
target_link_libraries(sdl2_opengl_demo PRIVATE SDL2::SDL2 SDL2::SDL2main glm::glm)

# OpenGL is provided by the platform (opengl32.lib on Windows).
if(WIN32)
    target_link_libraries(sdl2_opengl_demo PRIVATE opengl32)
endif()

# Copy the shaders next to the executable so they can be loaded at runtime.
add_custom_command(TARGET sdl2_opengl_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:sdl2_opengl_demo>/shaders
    COMMENT "Copying shaders next to the executable"
)
