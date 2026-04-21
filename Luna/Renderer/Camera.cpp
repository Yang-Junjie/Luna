#include "Camera.h"

#include <algorithm>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {

constexpr float kMinAspectRatio = 0.001f;
constexpr float kMinPerspectiveNear = 0.001f;
constexpr float kMinOrthoSize = 0.001f;
constexpr float kMinLookDirectionLengthSquared = 1.0e-6f;

glm::quat makeOrientation(const glm::vec3& euler_radians)
{
    const glm::quat pitch_rotation = glm::angleAxis(euler_radians.x, glm::vec3{1.0f, 0.0f, 0.0f});
    const glm::quat yaw_rotation = glm::angleAxis(euler_radians.y, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::quat roll_rotation = glm::angleAxis(euler_radians.z, glm::vec3{0.0f, 0.0f, 1.0f});
    return glm::normalize(yaw_rotation * pitch_rotation * roll_rotation);
}

} // namespace

namespace luna {

void Camera::setPosition(const glm::vec3& position)
{
    m_position = position;
}

const glm::vec3& Camera::getPosition() const
{
    return m_position;
}

void Camera::translateWorld(const glm::vec3& delta)
{
    m_position += delta;
}

void Camera::translateLocal(const glm::vec3& delta)
{
    m_position += getRightDirection() * delta.x;
    m_position += getUpDirection() * delta.y;
    m_position += getForwardDirection() * delta.z;
}

void Camera::setOrientationEuler(const glm::vec3& euler_radians)
{
    m_euler_radians = euler_radians;
}

void Camera::setYawPitchRoll(float yaw_radians, float pitch_radians, float roll_radians)
{
    m_euler_radians = glm::vec3(pitch_radians, yaw_radians, roll_radians);
}

glm::vec3 Camera::getOrientationEuler() const
{
    return m_euler_radians;
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up)
{
    const glm::vec3 direction = target - m_position;
    if (glm::length2(direction) <= kMinLookDirectionLengthSquared) {
        return;
    }

    const glm::vec3 normalized_direction = glm::normalize(direction);
    glm::vec3 normalized_up = glm::normalize(up);
    if (glm::length2(glm::cross(normalized_direction, normalized_up)) <= kMinLookDirectionLengthSquared) {
        normalized_up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::length2(glm::cross(normalized_direction, normalized_up)) <= kMinLookDirectionLengthSquared) {
            normalized_up = glm::vec3(0.0f, 0.0f, 1.0f);
        }
    }

    m_euler_radians = glm::eulerAngles(glm::quatLookAtRH(normalized_direction, normalized_up));
}

void Camera::setPerspective(float vertical_fov_radians, float near_clip, float far_clip)
{
    m_projection_type = ProjectionType::Perspective;
    m_perspective.vertical_fov_radians = vertical_fov_radians;
    m_perspective.near_clip = (std::max)(near_clip, kMinPerspectiveNear);
    m_perspective.far_clip = (std::max)(far_clip, m_perspective.near_clip + kMinPerspectiveNear);
}

void Camera::setOrthographic(float vertical_size, float near_clip, float far_clip)
{
    m_projection_type = ProjectionType::Orthographic;
    m_orthographic.vertical_size = (std::max)(vertical_size, kMinOrthoSize);
    m_orthographic.near_clip = (std::min)(near_clip, far_clip);
    m_orthographic.far_clip = (std::max)(far_clip, m_orthographic.near_clip + kMinPerspectiveNear);
}

Camera::ProjectionType Camera::getProjectionType() const
{
    return m_projection_type;
}

const Camera::PerspectiveSettings& Camera::getPerspectiveSettings() const
{
    return m_perspective;
}

const Camera::OrthographicSettings& Camera::getOrthographicSettings() const
{
    return m_orthographic;
}

glm::vec3 Camera::getForwardDirection() const
{
    return glm::normalize(makeOrientation(m_euler_radians) * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::getRightDirection() const
{
    return glm::normalize(makeOrientation(m_euler_radians) * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Camera::getUpDirection() const
{
    return glm::normalize(makeOrientation(m_euler_radians) * glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::getViewMatrix() const
{
    const glm::mat4 camera_translation = glm::translate(glm::mat4(1.0f), m_position);
    const glm::mat4 camera_rotation = glm::mat4_cast(makeOrientation(m_euler_radians));
    return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 Camera::getProjectionMatrix(float aspect_ratio) const
{
    const float clamped_aspect_ratio = (std::max)(aspect_ratio, kMinAspectRatio);
    if (m_projection_type == ProjectionType::Orthographic) {
        const float half_height = m_orthographic.vertical_size * 0.5f;
        const float half_width = half_height * clamped_aspect_ratio;
        return glm::orthoRH_ZO(-half_width,
                               half_width,
                               -half_height,
                               half_height,
                               m_orthographic.near_clip,
                               m_orthographic.far_clip);
    }

    return glm::perspectiveRH_ZO(
        m_perspective.vertical_fov_radians, clamped_aspect_ratio, m_perspective.near_clip, m_perspective.far_clip);
}

glm::mat4 Camera::getViewProjectionMatrix(float aspect_ratio) const
{
    return getProjectionMatrix(aspect_ratio) * getViewMatrix();
}

} // namespace luna
