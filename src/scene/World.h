#pragma once

#include <vector>

#include "scene/Components.h"
#include "scene/Scene.h"

// Owns the active scene and drives its per-frame updates.
class World
{
public:
    Scene&       activeScene()       { return m_activeScene; }
    const Scene& activeScene() const { return m_activeScene; }

    void update(float dt);

    // Convenience: render-facing light snapshots for the active scene.
    std::vector<Light> gatherLights() const;

private:
    Scene m_activeScene;
};
