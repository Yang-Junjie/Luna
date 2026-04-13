#pragma once

#include "Core/UUID.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <memory>
#include <string>
#include <utility>

namespace luna {

class Material;
class Mesh;

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
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    TransformComponent() = default;

    glm::mat4 getTransform() const
    {
        const glm::mat4 rotation_matrix = glm::toMat4(glm::quat(rotation));
        return glm::translate(glm::mat4(1.0f), translation) * rotation_matrix * glm::scale(glm::mat4(1.0f), scale);
    }
};

struct StaticMeshComponent {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool visible = true;

    StaticMeshComponent() = default;
    StaticMeshComponent(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material = {})
        : mesh(std::move(mesh))
        , material(std::move(material))
    {}
};

} // namespace luna
