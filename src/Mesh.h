#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "GL.h"
#include "core/Types.h"

// Interleaved vertex layout (position, normal, color) - see Mesh.cpp.
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

// Wraps a VAO/VBO (and optional EBO) for a single mesh.
class Mesh
{
public:
    Mesh(const std::vector<Vertex>& vertices,
         const std::vector<unsigned int>& indices = std::vector<unsigned int>());
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;

    // Local-space AABB computed from the vertex positions.
    const AABB& bounds() const { return m_bounds; }

private:
    gl::GLuint  m_vao = 0;
    gl::GLuint  m_vbo = 0;
    gl::GLuint  m_ebo = 0;
    gl::GLsizei m_count = 0;
    bool        m_indexed = false;
    AABB        m_bounds;
};
