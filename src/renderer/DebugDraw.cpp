#include "renderer/DebugDraw.h"

#include <cstddef>

#include "Shader.h"

namespace {

// The 12 edges of a box, as index pairs into the 8 corners selected by the
// (x, y, z) bit pattern used in addAABB().
constexpr int kEdges[24] = {
    0, 1,  1, 3,  3, 2,  2, 0,   // -z face
    4, 5,  5, 7,  7, 6,  6, 4,   // +z face
    0, 4,  1, 5,  2, 6,  3, 7,   // edges joining the two faces
};

} // namespace

DebugDraw::~DebugDraw()
{
    if (m_vbo) gl::DeleteBuffers(1, &m_vbo);
    if (m_vao) gl::DeleteVertexArrays(1, &m_vao);
}

void DebugDraw::begin()
{
    m_vertices.clear();
}

void DebugDraw::addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color)
{
    m_vertices.push_back(LineVertex{ a, color });
    m_vertices.push_back(LineVertex{ b, color });
}

void DebugDraw::addAABB(const AABB& box, const glm::vec3& color)
{
    if (!box.valid()) return;

    glm::vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = glm::vec3((i & 1) ? box.max.x : box.min.x,
                               (i & 2) ? box.max.y : box.min.y,
                               (i & 4) ? box.max.z : box.min.z);
    }

    for (int e = 0; e < 24; e += 2) {
        addLine(corners[kEdges[e]], corners[kEdges[e + 1]], color);
    }
}

void DebugDraw::flush(Shader& shader, const glm::mat4& view, const glm::mat4& projection)
{
    if (m_vertices.empty()) return;

    if (m_vao == 0) {
        gl::GenVertexArrays(1, &m_vao);
        gl::GenBuffers(1, &m_vbo);

        gl::BindVertexArray(m_vao);
        gl::BindBuffer(gl::ARRAY_BUFFER, m_vbo);

        const gl::GLsizei stride = sizeof(LineVertex);
        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(0, 3, gl::FLOAT, 0, stride,
                                reinterpret_cast<void*>(offsetof(LineVertex, position)));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(1, 3, gl::FLOAT, 0, stride,
                                reinterpret_cast<void*>(offsetof(LineVertex, color)));
    } else {
        gl::BindVertexArray(m_vao);
        gl::BindBuffer(gl::ARRAY_BUFFER, m_vbo);
    }

    const gl::GLsizeiptr bytes =
        static_cast<gl::GLsizeiptr>(m_vertices.size() * sizeof(LineVertex));
    gl::BufferData(gl::ARRAY_BUFFER, bytes, m_vertices.data(), gl::DYNAMIC_DRAW);

    shader.use();
    shader.setMat4("uView", view);
    shader.setMat4("uProjection", projection);

    gl::DrawArrays(gl::LINES, 0, static_cast<gl::GLsizei>(m_vertices.size()));

    gl::BindVertexArray(0);
}
