#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

glm::mat4 Camera::get_view_matrix() const
{
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position);
    const glm::mat4 cameraRotation = get_rotation_matrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::get_rotation_matrix() const
{
    const glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
    const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});

    return glm::mat4_cast(yawRotation) * glm::mat4_cast(pitchRotation);
}
