#include "Mesh.h"

#include <cstddef>

Mesh::Mesh(const std::vector<Vertex>& vertices,
           const std::vector<unsigned int>& indices)
    : m_count(static_cast<gl::GLsizei>(indices.empty() ? vertices.size()
                                                       : indices.size())),
      m_indexed(!indices.empty())
{
    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(1, &m_vbo);
    gl::BindBuffer(gl::ARRAY_BUFFER, m_vbo);
    gl::BufferData(gl::ARRAY_BUFFER,
                   static_cast<gl::GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                   vertices.data(), gl::STATIC_DRAW);

    const gl::GLsizei stride = sizeof(Vertex);

    // location 0: position
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 3, gl::FLOAT, 0, stride,
                            reinterpret_cast<void*>(offsetof(Vertex, position)));
    // location 1: normal
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 3, gl::FLOAT, 0, stride,
                            reinterpret_cast<void*>(offsetof(Vertex, normal)));
    // location 2: color
    gl::EnableVertexAttribArray(2);
    gl::VertexAttribPointer(2, 3, gl::FLOAT, 0, stride,
                            reinterpret_cast<void*>(offsetof(Vertex, color)));

    if (m_indexed) {
        gl::GenBuffers(1, &m_ebo);
        gl::BindBuffer(gl::ELEMENT_ARRAY_BUFFER, m_ebo);
        gl::BufferData(gl::ELEMENT_ARRAY_BUFFER,
                       static_cast<gl::GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                       indices.data(), gl::STATIC_DRAW);
    }

    gl::BindVertexArray(0);

    // Local-space bounds from the vertex positions.
    for (const Vertex& v : vertices) m_bounds.expand(v.position);
}

Mesh::~Mesh()
{
    if (m_ebo) gl::DeleteBuffers(1, &m_ebo);
    if (m_vbo) gl::DeleteBuffers(1, &m_vbo);
    if (m_vao) gl::DeleteVertexArrays(1, &m_vao);
}

void Mesh::draw() const
{
    gl::BindVertexArray(m_vao);
    if (m_indexed) {
        gl::DrawElements(gl::TRIANGLES, m_count, gl::UNSIGNED_INT, nullptr);
    } else {
        gl::DrawArrays(gl::TRIANGLES, 0, m_count);
    }
    gl::BindVertexArray(0);
}
