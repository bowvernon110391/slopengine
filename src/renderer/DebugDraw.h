#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "GL.h"
#include "core/Types.h"

class Shader;

// Batches world-space line segments and flushes them in a single draw call.
//
// Kept separate from Mesh because Mesh::draw() is hardwired to GL_TRIANGLES,
// and because a debug overlay wants one batched draw for the whole frame rather
// than one per box.
class DebugDraw
{
public:
    DebugDraw() = default;
    ~DebugDraw();

    DebugDraw(const DebugDraw&) = delete;
    DebugDraw& operator=(const DebugDraw&) = delete;

    // Clears the CPU-side vertex queue.
    void begin();

    void addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);

    // Queues the 12 edges of 'box'. Invalid boxes are ignored.
    void addAABB(const AABB& box, const glm::vec3& color);

    // Uploads and draws everything queued since begin(). Binds its own VAO and
    // the given shader, then leaves the VAO unbound. Draws nothing (and does not
    // touch GL) when the queue is empty.
    void flush(Shader& shader, const glm::mat4& view, const glm::mat4& projection);

private:
    struct LineVertex
    {
        glm::vec3 position;
        glm::vec3 color;
    };

    std::vector<LineVertex> m_vertices;
    gl::GLuint              m_vao = 0;
    gl::GLuint              m_vbo = 0;
};
