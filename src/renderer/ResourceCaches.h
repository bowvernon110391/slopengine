#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "core/Types.h"
#include "scene/Components.h"

class Mesh;

// Owns GPU meshes; MeshHandle.id is (index + 1), so 0 stays invalid.
class MeshCache
{
public:
    MeshHandle add(std::unique_ptr<Mesh> mesh);
    const Mesh* get(MeshHandle handle) const;
    std::size_t size() const { return m_meshes.size(); }

private:
    std::vector<std::unique_ptr<Mesh>> m_meshes;
};

// Owns materials; MaterialHandle.id is (index + 1).
class MaterialCache
{
public:
    MaterialHandle add(const Material& material);
    const Material* get(MaterialHandle handle) const;
    std::size_t size() const { return m_materials.size(); }

private:
    std::vector<Material> m_materials;
};
