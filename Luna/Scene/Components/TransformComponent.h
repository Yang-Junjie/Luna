#pragma once

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace luna {

struct TransformComponent {
    glm::vec3 translation{0.0f, 0.0f, 0.0f};
    // Euler angles in radians: pitch(x), yaw(y), roll(z)
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    TransformComponent() = default;

    explicit TransformComponent(const glm::vec3& translation)
        : translation(translation)
    {}

    TransformComponent(const TransformComponent&) = default;

    void setTransform(const glm::mat4& transform)
    {
        glm::vec3 skew{};
        glm::vec4 perspective{};
        glm::quat orientation{};
        glm::vec3 decomposed_scale{};
        glm::vec3 decomposed_translation{};

        if (!glm::decompose(transform, decomposed_scale, orientation, decomposed_translation, skew, perspective)) {
            return;
        }

        translation = decomposed_translation;
        rotation = glm::eulerAngles(glm::normalize(orientation));
        scale = decomposed_scale;
    }

    glm::vec3 getRotationEuler() const
    {
        return rotation;
    }

    void setRotationEuler(const glm::vec3& euler_radians)
    {
        rotation = euler_radians;
    }

    glm::mat4 getTransform() const
    {
        const glm::mat4 rotation_matrix = glm::toMat4(glm::quat(rotation));
        return glm::translate(glm::mat4(1.0f), translation) * rotation_matrix * glm::scale(glm::mat4(1.0f), scale);
    }

    glm::vec3 getForward() const
    {
        return glm::normalize(glm::rotate(glm::quat(rotation), glm::vec3(0.0f, 0.0f, -1.0f)));
    }
};

} // namespace luna
