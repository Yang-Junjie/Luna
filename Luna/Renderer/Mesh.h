#pragma once

// Defines mesh asset data used by scene submission and GPU upload paths.
// Holds vertex/index buffers in CPU memory and groups them into submeshes
// that the renderer can upload and draw independently.

#include "Asset/Asset.h"

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <vector>

namespace luna {

struct StaticMeshVertex {
    glm::vec3 Position{0.0f, 0.0f, 0.0f};
    glm::vec2 TexCoord{0.0f, 0.0f};
    glm::vec3 Normal{0.0f, 0.0f, 1.0f};
    glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};

    glm::vec3 Tangent{1.0f, 0.0f, 0.0f};
    glm::vec3 Bitangent{0.0f, 1.0f, 0.0f};
};

struct SubMesh {
    std::string Name;

    std::vector<StaticMeshVertex> Vertices;
    std::vector<uint32_t> Indices;

    uint32_t MaterialIndex{UINT32_MAX};
};

class Mesh final : public Asset {
public:
    Mesh() = default;

    explicit Mesh(std::string name, std::vector<SubMesh> subMeshes);

    static std::shared_ptr<Mesh> create(std::string name, std::vector<SubMesh> subMeshes);

    const std::string& getName() const;
    const std::vector<SubMesh>& getSubMeshes() const;

    AssetType getAssetsType() const override
    {
        return AssetType::Mesh;
    }

    bool isValid() const;

private:
    std::string m_name;
    std::vector<SubMesh> m_subMeshes;
};

} // namespace luna
