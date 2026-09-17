#include "scene/Scene.h"

#include <algorithm>

std::uint64_t Scene::s_nextSceneId = 1;

Scene::Scene()
{
    id = s_nextSceneId++;
}

Entity Scene::create()
{
    std::uint32_t id;
    if (!m_freeIds.empty()) {
        id = m_freeIds.back();
        m_freeIds.pop_back();
    } else {
        id = m_nextId++;
        if (m_alive.size() <= id) m_alive.resize(id + 1, false);
    }
    m_alive[id] = true;

    Entity e;
    e.id = id;
    m_entities.push_back(e);

    // Every entity starts with a transform.
    m_transforms.add(e, Transform{});
    return e;
}

void Scene::destroy(Entity e)
{
    if (!alive(e)) return;

    m_alive[e.id] = false;
    m_transforms.remove(e);
    m_meshes.remove(e);
    m_lights.remove(e);
    m_cameras.remove(e);

    m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), e),
                     m_entities.end());
    m_freeIds.push_back(e.id);
}

bool Scene::alive(Entity e) const
{
    return e.valid() && e.id < m_alive.size() && m_alive[e.id];
}

void Scene::computeWorldRecursive(Entity e, const glm::mat4& parentWorld)
{
    Transform* t = m_transforms.get(e);
    if (!t) return;

    t->worldMatrix = parentWorld * t->localMatrix();
    t->dirty = false;

    for (const Entity& child : t->children) {
        computeWorldRecursive(child, t->worldMatrix);
    }
}

void Scene::updateTransforms()
{
    // Roots = entities whose parent is unset or no longer in the scene.
    for (const Entity& e : m_entities) {
        const Transform* t = m_transforms.get(e);
        if (!t) continue;
        if (!t->parent.valid() || !m_transforms.has(t->parent)) {
            computeWorldRecursive(e, glm::mat4(1.0f));
        }
    }
}

void Scene::recomputeBounds()
{
    m_worldBounds.reset();

    const std::vector<MeshRenderer>& renderers = m_meshes.dense();
    const std::vector<Entity>&       owners    = m_meshes.entities();

    for (std::size_t i = 0; i < renderers.size(); ++i) {
        const Transform* t = m_transforms.get(owners[i]);
        const glm::mat4 world = t ? t->worldMatrix : glm::mat4(1.0f);
        m_worldBounds.merge(renderers[i].worldBounds.transformed(world));
    }
}

bool Scene::makeLight(Entity e, Light& out) const
{
    const LightComponent* lc = m_lights.get(e);
    if (!lc || lc->type == LightType::None) return false;

    const Transform* t = m_transforms.get(e);
    const glm::mat4 world = t ? t->worldMatrix : glm::mat4(1.0f);

    const glm::vec3 origin(world[3].x, world[3].y, world[3].z);
    const glm::vec3 localZ(world[2].x, world[2].y, world[2].z);

    out.type        = lc->type;
    out.position    = origin;
    out.direction   = glm::normalize(-localZ);
    out.color       = lc->color;
    out.intensity   = lc->intensity;
    out.range       = lc->range;
    out.innerAngle  = lc->innerAngle;
    out.outerAngle  = lc->outerAngle;
    out.castsShadow = lc->castsShadow;
    return true;
}
