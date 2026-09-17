#pragma once

#include <map>
#include <memory>
#include <string>

#include "Shader.h"

// Owns the shader programs shared across passes (Renderer::sharedShaders).
class ShaderCache
{
public:
    // Compile + link 'path' and store it under 'name'. Returns the Shader on
    // success, nullptr on failure.
    Shader* load(const std::string& name, const std::string& path);

    Shader* get(const std::string& name) const;

    void clear() { m_shaders.clear(); }

private:
    std::map<std::string, std::unique_ptr<Shader>> m_shaders;
};
