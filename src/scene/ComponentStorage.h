#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/Types.h"

// Sparse-set component storage: dense component array + parallel entity array
// + a sparse table mapping entity id -> dense index. This is the
// ComponentStorage<T> from the scene data model.
template <typename T>
class ComponentStorage
{
public:
    static constexpr std::uint32_t INVALID = 0xFFFFFFFFu;

    bool has(Entity e) const { return indexOf(e) != INVALID; }

    T* get(Entity e)
    {
        const std::uint32_t idx = indexOf(e);
        return (idx == INVALID) ? nullptr : &m_dense[idx];
    }

    const T* get(Entity e) const
    {
        const std::uint32_t idx = indexOf(e);
        return (idx == INVALID) ? nullptr : &m_dense[idx];
    }

    T& add(Entity e, const T& value = T{})
    {
        const std::uint32_t existing = indexOf(e);
        if (existing != INVALID) {
            m_dense[existing] = value;
            return m_dense[existing];
        }
        if (e.id >= m_sparse.size()) m_sparse.resize(e.id + 1, INVALID);
        m_sparse[e.id] = static_cast<std::uint32_t>(m_dense.size());
        m_dense.push_back(value);
        m_entities.push_back(e);
        return m_dense.back();
    }

    void remove(Entity e)
    {
        const std::uint32_t idx = indexOf(e);
        if (idx == INVALID) return;

        const std::uint32_t last = static_cast<std::uint32_t>(m_dense.size() - 1);
        if (idx != last) {
            m_dense[idx]    = m_dense[last];
            m_entities[idx] = m_entities[last];
            m_sparse[m_entities[idx].id] = idx;
        }
        m_dense.pop_back();
        m_entities.pop_back();
        m_sparse[e.id] = INVALID;
    }

    std::size_t size() const { return m_dense.size(); }
    bool        empty() const { return m_dense.empty(); }

    const std::vector<T>&      dense() const    { return m_dense; }
    std::vector<T>&            dense()          { return m_dense; }
    const std::vector<Entity>& entities() const { return m_entities; }

private:
    std::uint32_t indexOf(Entity e) const
    {
        if (e.id >= m_sparse.size()) return INVALID;
        const std::uint32_t idx = m_sparse[e.id];
        if (idx == INVALID || idx >= m_dense.size()) return INVALID;
        if (m_entities[idx] != e) return INVALID;
        return idx;
    }

    std::vector<T>             m_dense;     // packed components
    std::vector<Entity>        m_entities;  // owner of each dense slot
    std::vector<std::uint32_t> m_sparse;    // entity id -> dense index
};

// Out-of-line definition for the static constexpr member. Under C++11/14 an
// ODR-use (e.g. binding INVALID to the `const T&` parameter of
// std::vector::resize) requires a definition with storage. MSVC accepts the
// in-class declaration alone, but GCC/Clang do not, so provide it explicitly.
template <typename T>
constexpr std::uint32_t ComponentStorage<T>::INVALID;
