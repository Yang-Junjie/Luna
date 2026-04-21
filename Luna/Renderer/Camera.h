#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace luna {

class Camera {
public:
    enum class ProjectionType {
        Perspective,
        Orthographic,
    };

    struct PerspectiveSettings {
        float vertical_fov_radians{0.872664626f};
        float near_clip{0.05f};
        float far_clip{200.0f};
    };

    struct OrthographicSettings {
        float vertical_size{10.0f};
        float near_clip{-100.0f};
        float far_clip{100.0f};
    };

    Camera() = default;

    void setPosition(const glm::vec3& position);
    const glm::vec3& getPosition() const;

    void translateWorld(const glm::vec3& delta);
    void translateLocal(const glm::vec3& delta);

    void setOrientationEuler(const glm::vec3& euler_radians);
    void setYawPitchRoll(float yaw_radians, float pitch_radians, float roll_radians = 0.0f);
    glm::vec3 getOrientationEuler() const;

    void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

    void setPerspective(float vertical_fov_radians, float near_clip, float far_clip);
    void setOrthographic(float vertical_size, float near_clip, float far_clip);
    ProjectionType getProjectionType() const;

    const PerspectiveSettings& getPerspectiveSettings() const;
    const OrthographicSettings& getOrthographicSettings() const;

    glm::vec3 getForwardDirection() const;
    glm::vec3 getRightDirection() const;
    glm::vec3 getUpDirection() const;

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect_ratio) const;
    glm::mat4 getViewProjectionMatrix(float aspect_ratio) const;

private:
    ProjectionType m_projection_type{ProjectionType::Perspective};
    PerspectiveSettings m_perspective{};
    OrthographicSettings m_orthographic{};
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};
    glm::vec3 m_euler_radians{0.0f, 0.0f, 0.0f};
};

} // namespace luna
