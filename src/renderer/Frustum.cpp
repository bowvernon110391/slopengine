#include "renderer/Frustum.h"

void Frustum::fromViewProj(const glm::mat4& m)
{
    // GLM matrices are column-major: m[col][row]. Row i of the matrix is
    // therefore (m[0][i], m[1][i], m[2][i], m[3][i]).
    const glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
    const glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
    const glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
    const glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

    planes[0] = row3 + row0;   // left
    planes[1] = row3 - row0;   // right
    planes[2] = row3 + row1;   // bottom
    planes[3] = row3 - row1;   // top
    planes[4] = row3 + row2;   // near
    planes[5] = row3 - row2;   // far

    for (glm::vec4& p : planes) {
        const float len = glm::length(glm::vec3(p));
        if (len > 0.0f) p /= len;
    }
}

bool Frustum::intersects(const AABB& box) const
{
    // No usable bounds: treat as visible rather than culling something we
    // cannot measure.
    if (!box.valid()) return true;

    for (const glm::vec4& p : planes) {
        // Positive vertex: the corner furthest along the plane normal. If even
        // that corner is behind the plane, the whole box is outside.
        const glm::vec3 pv(p.x >= 0.0f ? box.max.x : box.min.x,
                           p.y >= 0.0f ? box.max.y : box.min.y,
                           p.z >= 0.0f ? box.max.z : box.min.z);

        if (glm::dot(glm::vec3(p), pv) + p.w < 0.0f) return false;
    }
    return true;
}
