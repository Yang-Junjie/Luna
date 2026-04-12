#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

glm::mat4 Camera::getViewMatrix() const
{
    const glm::mat4 camera_translation = glm::translate(glm::mat4(1.0f), m_position);
    const glm::mat4 camera_rotation = getRotationMatrix();
    return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 Camera::getRotationMatrix() const
{
    const glm::quat pitch_rotation = glm::angleAxis(m_pitch, glm::vec3{1.f, 0.f, 0.f});
    const glm::quat yaw_rotation = glm::angleAxis(m_yaw, glm::vec3{0.f, -1.f, 0.f});

    return glm::mat4_cast(yaw_rotation) * glm::mat4_cast(pitch_rotation);
}
