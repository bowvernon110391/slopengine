#include "scene/World.h"

void World::update(float dt)
{
    (void)dt;
    m_activeScene.updateTransforms();
    m_activeScene.recomputeBounds();
}

std::vector<Light> World::gatherLights() const
{
    std::vector<Light> lights;
    const ComponentStorage<LightComponent>& storage = m_activeScene.lights();
    const std::vector<Entity>& owners = storage.entities();

    lights.reserve(owners.size());
    for (const Entity& e : owners) {
        Light l;
        if (m_activeScene.makeLight(e, l)) lights.push_back(l);
    }
    return lights;
}
