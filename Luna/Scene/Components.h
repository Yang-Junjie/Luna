#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"
#include "Renderer/Camera.h"

#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <utility>
#include <vector>

namespace luna {

struct IDComponent {
    UUID id;

    IDComponent() = default;

    explicit IDComponent(UUID id)
        : id(id)
    {}
};

struct TagComponent {
    std::string tag;

    TagComponent() = default;

    explicit TagComponent(std::string tag)
        : tag(std::move(tag))
    {}
};

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

        if (!glm::decompose(transform,
                            decomposed_scale,
                            orientation,
                            decomposed_translation,
                            skew,
                            perspective)) {
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

struct CameraComponent {
    bool primary = true;
    bool fixedAspectRatio = false;
    Camera::ProjectionType projectionType = Camera::ProjectionType::Perspective;
    float perspectiveVerticalFovRadians = glm::radians(50.0f);
    float perspectiveNear = 0.05f;
    float perspectiveFar = 500.0f;
    float orthographicSize = 10.0f;
    float orthographicNear = -100.0f;
    float orthographicFar = 100.0f;

    CameraComponent() = default;
    CameraComponent(const CameraComponent&) = default;

    Camera createCamera() const
    {
        Camera camera;
        if (projectionType == Camera::ProjectionType::Orthographic) {
            camera.setOrthographic(orthographicSize, orthographicNear, orthographicFar);
        } else {
            camera.setPerspective(perspectiveVerticalFovRadians, perspectiveNear, perspectiveFar);
        }
        return camera;
    }
};

struct LightComponent {
    enum class Type : uint8_t {
        Directional = 0,
        Point = 1,
        Spot = 2,
    };

    Type type = Type::Directional;
    bool enabled = true;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 4.0f;
    float range = 10.0f;
    float innerConeAngleRadians = glm::radians(20.0f);
    float outerConeAngleRadians = glm::radians(35.0f);

    LightComponent() = default;
    LightComponent(const LightComponent&) = default;
};

struct RelationshipComponent {
    UUID parentHandle = UUID(0);
    std::vector<UUID> children;

    RelationshipComponent() = default;
    RelationshipComponent(const RelationshipComponent&) = default;
    explicit RelationshipComponent(UUID parent)
        : parentHandle(parent)
    {}
};

struct MeshComponent {
    AssetHandle meshHandle = AssetHandle(0);

    std::vector<AssetHandle> submeshMaterials;

    MeshComponent() = default;

    MeshComponent(const MeshComponent&) = default;

    void setSubmeshMaterial(uint32_t submeshIndex, AssetHandle materialHandle)
    {
        if (submeshIndex >= submeshMaterials.size()) {
            submeshMaterials.resize(submeshIndex + 1, AssetHandle(0));
        }
        submeshMaterials[submeshIndex] = materialHandle;
    }

    AssetHandle getSubmeshMaterial(uint32_t submeshIndex) const
    {
        if (submeshIndex < submeshMaterials.size()) {
            return submeshMaterials[submeshIndex];
        }
        return AssetHandle(0);
    }

    void clearSubmeshMaterial(uint32_t submeshIndex)
    {
        if (submeshIndex < submeshMaterials.size()) {
            submeshMaterials[submeshIndex] = AssetHandle(0);
        }
    }

    void clearAllSubmeshMaterials()
    {
        submeshMaterials.clear();
    }

    size_t getSubmeshMaterialCount() const
    {
        return submeshMaterials.size();
    }

    void resizeSubmeshMaterials(size_t count)
    {
        submeshMaterials.resize(count, AssetHandle(0));
    }
};

} // namespace luna
