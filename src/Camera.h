#pragma once

#include <glm/glm.hpp>

// A simple first-person style camera.
class Camera
{
public:
    Camera();

    void setAspect(float aspect) { m_aspect = aspect; }

    // forward/right/up are -1, 0 or +1 direction inputs.
    void processKeyboard(float forward, float right, float up, float dt);

    // dx/dy are accumulated mouse deltas (pixels) for this frame.
    void processMouse(float dx, float dy);

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix() const;
    glm::vec3 position() const { return m_position; }

    void  setMoveSpeed(float s) { m_moveSpeed = s; }
    float moveSpeed() const { return m_moveSpeed; }

    void  setMouseSensitivity(float s) { m_mouseSensitivity = s; }
    float mouseSensitivity() const { return m_mouseSensitivity; }

    void  setPosition(const glm::vec3& p) { m_position = p; }

    float yaw() const   { return m_yaw; }
    float pitch() const { return m_pitch; }
    void  setYawPitch(float yaw, float pitch);   // clamps pitch, rebuilds vectors

    float fovDegrees() const { return m_fovY * (180.0f / 3.14159265358979323846f); }
    float fovRadians() const { return m_fovY; }
    void  setFovDegrees(float deg);

    float aspect() const { return m_aspect; }
    float zNear()  const { return m_zNear; }
    float zFar()   const { return m_zFar; }

private:
    void updateVectors();

    glm::vec3  m_position;
    float m_yaw;    // radians around +Y
    float m_pitch;  // radians around +X

    float m_moveSpeed;
    float m_mouseSensitivity;

    glm::vec3 m_forward;
    glm::vec3 m_right;
    glm::vec3 m_up;

    float m_fovY;   // radians
    float m_aspect;
    float m_zNear;
    float m_zFar;
};
