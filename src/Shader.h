#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "GL.h"

// Wraps a compiled + linked OpenGL shader program.
//
// Shaders are loaded from a SINGLE file that contains both stages, separated
// by "#type vertex" and "#type fragment" markers. The file (and any file it
// #includes, recursively) may contain reusable GLSL code.
//
// Example:
//
//     #type vertex
//     #version 330 core
//     layout(location = 0) in vec3 aPos;
//     void main() { gl_Position = vec4(aPos, 1.0); }
//
//     #type fragment
//     #version 330 core
//     #include "common/color.glsl"
//     out vec4 FragColor;
//     void main() { FragColor = vec4(1.0); }
class Shader
{
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Load + compile + link a shader from a single .glsl file (both stages).
    // #include directives are resolved relative to the including file and may
    // nest. Returns false on failure (missing file, include cycle, compile or
    // link error).
    bool load(const std::string& path);

    // Inject a #define flag into every stage before compilation. Useful for
    // toggling features from C++ (e.g. addDefine("USE_NORMAL_MAPPING")).
    void addDefine(const std::string& name, const std::string& value = "1");

    // Query the #define flags found in the loaded source (including files).
    bool hasDefine(const std::string& name) const;
    const std::vector<std::string>& defines() const { return m_defines; }

    void use() const;

    void setMat4(const char* name, const glm::mat4& mat) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setVec4(const char* name, const glm::vec4& v) const;
    void setFloat(const char* name, float v) const;
    void setInt(const char* name, int v) const;

    // Uniform arrays (e.g. point-light position/color/intensity/range arrays).
    void setVec3Array(const char* name, int count, const glm::vec3* values) const;
    void setFloatArray(const char* name, int count, const float* values) const;

    bool valid() const { return m_program != 0; }

private:
    // Resolve (and cache) the location of a uniform by name. Locations belong to
    // the linked program, so the cache is cleared whenever a program is built.
    // Misses are cached as -1 too, so a typo costs one lookup, not one per frame.
    gl::GLint location(const char* name) const;

    gl::GLuint m_program = 0;

    mutable std::unordered_map<std::string, gl::GLint> m_uniformCache;

    // Names of #defines discovered in the source (deduplicated, in order).
    std::vector<std::string> m_defines;

    // Fully-formed "#define ..." lines to prepend before compilation.
    std::vector<std::string> m_injectedDefines;
};
