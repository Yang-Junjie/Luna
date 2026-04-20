#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"

#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
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
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    TransformComponent() = default;

    glm::mat4 getTransform() const
    {
        const glm::mat4 rotation_matrix = glm::toMat4(glm::quat(rotation));
        return glm::translate(glm::mat4(1.0f), translation) * rotation_matrix * glm::scale(glm::mat4(1.0f), scale);
    }
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
