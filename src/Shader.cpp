#include "Shader.h"

#include <fstream>
#include <sstream>

#include <glm/gtc/type_ptr.hpp>

namespace {

// --- File / path helpers ---------------------------------------------------

std::string readFile(const std::string& path)
{
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in) return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The directory part of a path (including the trailing separator), or "".
std::string dirName(const std::string& path)
{
    std::size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : path.substr(0, pos + 1);
}

std::string trimLeft(const std::string& s)
{
    std::size_t p = s.find_first_not_of(" \t\r");
    return (p == std::string::npos) ? std::string() : s.substr(p);
}

// Trim leading AND trailing whitespace. The trailing '\r' matters because
// shader files may use CRLF line endings, and getline() leaves the '\r' on
// each line (e.g. "#type vertex\r").
std::string trim(const std::string& s)
{
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Extract the target of an "#include" directive (quoted or angle-bracketed).
// Returns false if the line does not look like a valid include.
bool parseIncludeTarget(const std::string& line, std::string& out)
{
    std::size_t a = line.find('"');
    if (a != std::string::npos) {
        std::size_t b = line.find('"', a + 1);
        if (b == std::string::npos) return false;
        out = line.substr(a + 1, b - a - 1);
        return !out.empty();
    }
    std::size_t c = line.find('<');
    if (c != std::string::npos) {
        std::size_t d = line.find('>', c + 1);
        if (d == std::string::npos) return false;
        out = line.substr(c + 1, d - c - 1);
        return !out.empty();
    }
    return false;
}

// --- #include preprocessing ------------------------------------------------

// Recursively expand "#include" directives. "absPath" must be absolute-ish
// (we resolve relative includes against its directory). "stack" holds the
// chain of files currently being expanded so we can detect cycles.
bool expandIncludes(const std::string& absPath,
                    std::vector<std::string>& stack,
                    int depth,
                    std::string& out)
{
    if (depth > 32) {
        SDL_Log("Shader include depth exceeded (circular includes?) at '%s'.",
                absPath.c_str());
        return false;
    }
    for (const std::string& p : stack) {
        if (p == absPath) {
            SDL_Log("Shader include cycle detected at '%s'.", absPath.c_str());
            return false;
        }
    }

    std::string src = readFile(absPath);
    if (src.empty()) {
        SDL_Log("Shader file not found or empty: '%s'.", absPath.c_str());
        return false;
    }

    stack.push_back(absPath);

    std::istringstream in(src);
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trimLeft(line);
        if (t.compare(0, 8, "#include") == 0) {
            std::string target;
            if (!parseIncludeTarget(t, target)) {
                SDL_Log("Malformed #include in '%s': '%s'.",
                        absPath.c_str(), line.c_str());
                stack.pop_back();
                return false;
            }
            std::string included = dirName(absPath) + target;
            std::string expanded;
            if (!expandIncludes(included, stack, depth + 1, expanded)) {
                stack.pop_back();
                return false;
            }
            out += expanded;
            out += '\n';
        } else {
            out += line;
            out += '\n';
        }
    }

    stack.pop_back();
    return true;
}

// --- Stage splitting -------------------------------------------------------

// Split the combined source on "#type vertex" / "#type fragment" markers.
// Anything before the first marker is ignored (comments, etc.).
bool splitStages(const std::string& combined, std::string& vert, std::string& frag)
{
    enum Stage { None, Vertex, Fragment };
    Stage cur = None;

    std::istringstream in(combined);
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.compare(0, 5, "#type") == 0) {
            std::string stage = trim(t.substr(5));
            if (stage == "vertex" || stage == "vert")        cur = Vertex;
            else if (stage == "fragment" || stage == "frag") cur = Fragment;
            continue;
        }
        if (cur == Vertex)       vert += line + '\n';
        else if (cur == Fragment) frag += line + '\n';
    }

    return !vert.empty() && !frag.empty();
}

// --- #define discovery -----------------------------------------------------

// Collect the names of every object-like / function-like macro defined in the
// source. Names are deduplicated and kept in first-seen order.
void collectDefines(const std::string& source, std::vector<std::string>& out)
{
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.compare(0, 7, "#define") != 0) continue;
        std::string rest = trim(t.substr(7));
        if (rest.empty()) continue;
        // Macro name runs until whitespace or '(' (function-like macros).
        std::size_t p = rest.find_first_of(" \t(");
        std::string name = (p == std::string::npos) ? rest : rest.substr(0, p);
        if (name.empty()) continue;
        bool seen = false;
        for (const std::string& e : out) if (e == name) { seen = true; break; }
        if (!seen) out.push_back(name);
    }
}

// --- Compilation -----------------------------------------------------------

bool compileShader(gl::GLenum type, const std::string& src, gl::GLuint& out)
{
    out = gl::CreateShader(type);
    const char* cstr = src.c_str();
    gl::ShaderSource(out, 1, &cstr, nullptr);
    gl::CompileShader(out);

    gl::GLint ok = 0;
    gl::GetShaderiv(out, gl::COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        gl::GLsizei len = 0;
        gl::GetShaderInfoLog(out, sizeof(log), &len, log);
        SDL_Log("Shader compile error (%s):\n%s",
                type == gl::VERTEX_SHADER ? "vertex" : "fragment", log);
        return false;
    }
    return true;
}

// Insert "defines" immediately after the "#version ..." line (it must remain
// the first line), or at the very top if there is no #version directive.
void injectDefines(std::string& src, const std::string& block)
{
    if (block.empty()) return;
    std::size_t pos = src.find("#version");
    if (pos == std::string::npos) {
        src = block + src;
        return;
    }
    std::size_t eol = src.find('\n', pos);
    if (eol == std::string::npos) eol = src.size();
    src.insert(eol + 1, block);
}

} // namespace

Shader::~Shader()
{
    if (m_program) gl::DeleteProgram(m_program);
}

bool Shader::load(const std::string& path)
{
    // Locations are owned by the linked program, so they cannot outlive it.
    m_uniformCache.clear();

    // 1) Recursively expand #includes into a single combined source string.
    std::vector<std::string> stack;
    std::string combined;
    if (!expandIncludes(path, stack, 0, combined)) return false;

    // 2) Split the combined source into vertex + fragment stages.
    std::string vert, frag;
    if (!splitStages(combined, vert, frag)) {
        SDL_Log("Shader '%s' must contain both '#type vertex' and "
                "'#type fragment' sections.", path.c_str());
        return false;
    }

    // 3) Record the #defines present in the source (for flag introspection).
    m_defines.clear();
    collectDefines(combined, m_defines);

    // 4) Build the block of injected #defines (from addDefine()).
    std::string defineBlock;
    for (const std::string& d : m_injectedDefines) defineBlock += d;
    injectDefines(vert, defineBlock);
    injectDefines(frag, defineBlock);

    // 5) Compile both stages.
    gl::GLuint vs = 0, fs = 0;
    if (!compileShader(gl::VERTEX_SHADER, vert, vs)) return false;
    if (!compileShader(gl::FRAGMENT_SHADER, frag, fs)) {
        gl::DeleteShader(vs);
        return false;
    }

    // 6) Link.
    m_program = gl::CreateProgram();
    gl::AttachShader(m_program, vs);
    gl::AttachShader(m_program, fs);
    gl::LinkProgram(m_program);

    // Shaders can be deleted once linked.
    gl::DeleteShader(vs);
    gl::DeleteShader(fs);

    gl::GLint ok = 0;
    gl::GetProgramiv(m_program, gl::LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        gl::GLsizei len = 0;
        gl::GetProgramInfoLog(m_program, sizeof(log), &len, log);
        SDL_Log("Program link error:\n%s", log);
        gl::DeleteProgram(m_program);
        m_program = 0;
        return false;
    }
    return true;
}

void Shader::addDefine(const std::string& name, const std::string& value)
{
    // Store the raw directive so it is ready to prepend verbatim.
    m_injectedDefines.push_back("#define " + name + " " + value + "\n");
}

bool Shader::hasDefine(const std::string& name) const
{
    for (const std::string& d : m_defines) if (d == name) return true;
    return false;
}

gl::GLint Shader::location(const char* name) const
{
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;

    const gl::GLint loc = gl::GetUniformLocation(m_program, name);
    m_uniformCache.emplace(name, loc);
    return loc;
}

void Shader::use() const
{
    gl::UseProgram(m_program);
}

void Shader::setMat4(const char* name, const glm::mat4& mat) const
{
    const gl::GLint loc = location(name);
    if (loc < 0) return;
    gl::UniformMatrix4fv(loc, 1, 0, glm::value_ptr(mat));
}

void Shader::setVec3(const char* name, const glm::vec3& v) const
{
    const gl::GLint loc = location(name);
    if (loc < 0) return;
    gl::Uniform3fv(loc, 1, glm::value_ptr(v));
}

void Shader::setVec4(const char* name, const glm::vec4& v) const
{
    const gl::GLint loc = location(name);
    if (loc < 0) return;
    gl::Uniform4fv(loc, 1, glm::value_ptr(v));
}

void Shader::setFloat(const char* name, float v) const
{
    const gl::GLint loc = location(name);
    if (loc < 0) return;
    gl::Uniform1f(loc, v);
}

void Shader::setInt(const char* name, int v) const
{
    const gl::GLint loc = location(name);
    if (loc < 0) return;
    gl::Uniform1i(loc, v);
}

void Shader::setVec3Array(const char* name, int count, const glm::vec3* values) const
{
    if (count <= 0 || !values) return;
    const gl::GLint loc = location(name);
    if (loc < 0) return;
    gl::Uniform3fv(loc, count, glm::value_ptr(values[0]));
}

void Shader::setFloatArray(const char* name, int count, const float* values) const
{
    if (count <= 0 || !values) return;
    const gl::GLint loc = location(name);
    if (loc < 0) return;
    gl::Uniform1fv(loc, count, values);
}
