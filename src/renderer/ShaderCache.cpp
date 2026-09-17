#include "renderer/ShaderCache.h"

#include <utility>

Shader* ShaderCache::load(const std::string& name, const std::string& path)
{
    std::unique_ptr<Shader> shader(new Shader());
    if (!shader->load(path)) return nullptr;

    Shader* raw = shader.get();
    m_shaders[name] = std::move(shader);
    return raw;
}

Shader* ShaderCache::get(const std::string& name) const
{
    auto it = m_shaders.find(name);
    return (it == m_shaders.end()) ? nullptr : it->second.get();
}
