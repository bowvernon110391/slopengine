#include "renderer/ResourceCaches.h"

#include <utility>

#include "Mesh.h"

MeshHandle MeshCache::add(std::unique_ptr<Mesh> mesh)
{
    m_meshes.push_back(std::move(mesh));
    MeshHandle handle;
    handle.id = static_cast<std::uint32_t>(m_meshes.size());
    return handle;
}

const Mesh* MeshCache::get(MeshHandle handle) const
{
    if (!handle.valid() || handle.id > m_meshes.size()) return nullptr;
    return m_meshes[handle.id - 1].get();
}

MaterialHandle MaterialCache::add(const Material& material)
{
    m_materials.push_back(material);
    MaterialHandle handle;
    handle.id = static_cast<std::uint32_t>(m_materials.size());
    return handle;
}

const Material* MaterialCache::get(MaterialHandle handle) const
{
    if (!handle.valid() || handle.id > m_materials.size()) return nullptr;
    return &m_materials[handle.id - 1];
}
