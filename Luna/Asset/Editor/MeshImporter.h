#pragma once
#include "Importer.h"

namespace luna {

namespace mesh_importer_detail {

inline YAML::Node makeDefaultMeshConfig()
{
    YAML::Node config(YAML::NodeType::Map);
    config["GenerateTangents"] = true;
    config["Triangulate"] = true;
    return config;
}

} // namespace mesh_importer_detail

class MeshImporter final : public Importer {
public:
    ~MeshImporter() override = default;

    AssetMetadata import(const std::filesystem::path& mesh_path) override
    {
        AssetMetadata metadata = importer_detail::makeAssetMetadata(mesh_path, AssetType::Mesh);
        metadata.SpecializedConfig = mesh_importer_detail::makeDefaultMeshConfig();
        return metadata;
    }

    std::vector<std::string> getSupportedExtensions() const override
    {
        return {".obj", ".fbx", ".gltf", ".glb"};
    }

    void serializeMetadata(const AssetMetadata& metadata) override
    {
        importer_detail::serializeMetadataWithConfig(metadata, [](const AssetMetadata&) {
            return mesh_importer_detail::makeDefaultMeshConfig();
        });
    }

    AssetMetadata deserializeMetadata(const std::filesystem::path& metaPath) override
    {
        return importer_detail::deserializeMetadataWithConfig(metaPath, [](const AssetMetadata&) {
            return mesh_importer_detail::makeDefaultMeshConfig();
        });
    }
};
} // namespace luna
