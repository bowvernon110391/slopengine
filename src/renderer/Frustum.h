#pragma once

#include <glm/glm.hpp>

#include "core/Types.h"

// Six-plane view frustum, extracted from a combined view-projection matrix
// (Gribb-Hartmann). Plane normals point inward, so a point is inside when
// dot(normal, p) + w >= 0 for every plane.
struct Frustum
{
    // xyz = inward normal, w = plane offset.
    glm::vec4 planes[6];

    void fromViewProj(const glm::mat4& viewProj);

    // Conservative AABB test: returns false only when the box is definitely
    // outside. Boxes straddling a plane are kept, so partially visible objects
    // are never dropped.
    bool intersects(const AABB& box) const;
};
