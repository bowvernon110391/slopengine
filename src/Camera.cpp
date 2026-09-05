#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
}

Camera::Camera()
    : m_position(0.0f, 2.0f, 8.0f),
      m_yaw(0.0f),
      m_pitch(0.0f),
      m_moveSpeed(6.0f),
      m_mouseSensitivity(0.0018f),
      m_fovY(45.0f * DEG2RAD),
      m_aspect(16.0f / 9.0f),
      m_zNear(0.1f),
      m_zFar(100.0f)
{
    updateVectors();
}

void Camera::updateVectors()
{
    glm::vec3 f;
    f.x = std::cos(m_pitch) * std::sin(m_yaw);
    f.y = std::sin(m_pitch);
    f.z = -std::cos(m_pitch) * std::cos(m_yaw);
    m_forward = glm::normalize(f);

    // right = forward x world-up
    m_right = glm::normalize(glm::cross(m_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up    = glm::cross(m_right, m_forward);
}

void Camera::processKeyboard(float forward, float right, float up, float dt)
{
    glm::vec3 dir = m_forward * forward + m_right * right + glm::vec3(0.0f, 1.0f, 0.0f) * up;
    if (glm::dot(dir, dir) > 0.0f) {
        dir = glm::normalize(dir);
        m_position += dir * (m_moveSpeed * dt);
    }
}

void Camera::processMouse(float dx, float dy)
{
    m_yaw   += dx * m_mouseSensitivity;
    m_pitch -= dy * m_mouseSensitivity;   // negate: SDL yrel grows downward

    constexpr float LIMIT = 89.0f * DEG2RAD;
    if (m_pitch >  LIMIT) m_pitch =  LIMIT;
    if (m_pitch < -LIMIT) m_pitch = -LIMIT;

    updateVectors();
}

glm::mat4 Camera::viewMatrix() const
{
    return glm::lookAt(m_position, m_position + m_forward, m_up);
}

glm::mat4 Camera::projectionMatrix() const
{
    return glm::perspective(m_fovY, m_aspect, m_zNear, m_zFar);
}

void Camera::setYawPitch(float yaw, float pitch)
{
    m_yaw = yaw;
    m_pitch = pitch;

    constexpr float LIMIT = 89.0f * DEG2RAD;
    if (m_pitch >  LIMIT) m_pitch =  LIMIT;
    if (m_pitch < -LIMIT) m_pitch = -LIMIT;

    updateVectors();
}

void Camera::setFovDegrees(float deg)
{
    m_fovY = deg * DEG2RAD;
}
