#pragma once

#include "Asset/Asset.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace luna {

struct ModelNode {
    std::string Name;
    int32_t Parent = -1;
    std::vector<uint32_t> Children;

    glm::vec3 Translation{0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f, 1.0f, 1.0f};

    AssetHandle MeshHandle{0};
    std::vector<AssetHandle> SubmeshMaterials;
};

class Model final : public Asset {
public:
    Model() = default;

    Model(std::string name, std::filesystem::path source, std::vector<ModelNode> nodes);

    static std::shared_ptr<Model> create(std::string name,
                                         std::filesystem::path source,
                                         std::vector<ModelNode> nodes);

    const std::string& getName() const;
    const std::filesystem::path& getSourcePath() const;
    const std::vector<ModelNode>& getNodes() const;

    AssetType getAssetsType() const override
    {
        return AssetType::Model;
    }

    bool isValid() const;

private:
    std::string m_name;
    std::filesystem::path m_source;
    std::vector<ModelNode> m_nodes;
};

} // namespace luna
