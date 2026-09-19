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

    // The cached static bounds described this entity, so they are now stale.
    // Without this, recomputeBounds() would keep serving a box that still contains
    // the destroyed entity's contribution -- which flows into the shadow fit's
    // extrusion and so into the light's ortho depth range. Measured: with this
    // omitted, worldBounds().max.y stayed frozen at the first field's value across
    // 25 respawns instead of tracking each new field.
    m_boundsDirty = true;
}

bool Scene::alive(Entity e) const
{
    return e.valid() && e.id < m_alive.size() && m_alive[e.id];
}

void Scene::computeWorldRecursive(Entity e, const glm::mat4& parentWorld, bool parentChanged)
{
    Transform* t = m_transforms.get(e);
    if (!t) return;

    // A frozen node under an unchanged parent cannot have moved, so neither can
    // anything below it: the whole subtree is already up to date.
    if (!parentChanged && t->isStatic && !t->dirty) return;

    t->worldMatrix = parentWorld * t->localMatrix();
    t->dirty = false;

    // Everything below a node we just recomputed has to be revisited, even if it
    // is itself static -- it may have been dragged along by this node.
    for (const Entity& child : t->children) {
        computeWorldRecursive(child, t->worldMatrix, true);
    }
}

void Scene::updateTransforms()
{
    // Roots = entities whose parent is unset or no longer in the scene.
    for (const Entity& e : m_entities) {
        const Transform* t = m_transforms.get(e);
        if (!t) continue;
        if (!t->parent.valid() || !m_transforms.has(t->parent)) {
            computeWorldRecursive(e, glm::mat4(1.0f), false);
        }
    }
}

void Scene::markDirtyRecursive(Entity e)
{
    Transform* t = m_transforms.get(e);
    if (!t) return;

    t->dirty = true;

    // Any cached world-space bounds in this subtree are now stale.
    if (MeshRenderer* mr = m_meshes.get(e)) {
        mr->cachedWorldBounds.reset();
    }

    for (const Entity& child : t->children) {
        markDirtyRecursive(child);
    }
}

void Scene::markTransformDirty(Entity e)
{
    markDirtyRecursive(e);
    m_boundsDirty = true;
}

void Scene::recomputeBounds()
{
    const std::vector<MeshRenderer>& renderers = m_meshes.dense();
    const std::vector<Entity>&       owners    = m_meshes.entities();

    // Frozen transforms keep their world matrix, so their contribution only has
    // to be rebuilt when something was actually marked dirty.
    if (m_boundsDirty) {
        m_staticBounds.reset();
        for (std::size_t i = 0; i < renderers.size(); ++i) {
            const Transform* t = m_transforms.get(owners[i]);
            if (!t || !t->isStatic || t->dirty) continue;   // dynamic: per-frame
            m_staticBounds.merge(renderers[i].worldBounds.transformed(t->worldMatrix));
        }
        m_boundsDirty = false;
    }

    m_worldBounds = m_staticBounds;

    for (std::size_t i = 0; i < renderers.size(); ++i) {
        const Transform* t = m_transforms.get(owners[i]);
        if (t && t->isStatic && !t->dirty) continue;        // already merged above
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
