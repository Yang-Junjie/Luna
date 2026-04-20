#pragma once

#include "Importer.h"

namespace luna {

namespace material_importer_detail {

inline YAML::Node makeDefaultMaterialConfig()
{
    YAML::Node config(YAML::NodeType::Map);
    config["UseMaterialFileParameters"] = true;
    return config;
}

} // namespace material_importer_detail

class MaterialImporter final : public Importer {
public:
    AssetMetadata import(const std::filesystem::path& material_path) override
    {
        AssetMetadata metadata = importer_detail::makeAssetMetadata(material_path, AssetType::Material);
        metadata.SpecializedConfig = material_importer_detail::makeDefaultMaterialConfig();
        return metadata;
    }

    std::vector<std::string> getSupportedExtensions() const override
    {
        return {".mtl", ".material", ".lmat", ".lunamat"};
    }

    void serializeMetadata(const AssetMetadata& metadata) override
    {
        importer_detail::serializeMetadataWithConfig(metadata, [](const AssetMetadata&) {
            return material_importer_detail::makeDefaultMaterialConfig();
        });
    }

    AssetMetadata deserializeMetadata(const std::filesystem::path& meta_path) override
    {
        return importer_detail::deserializeMetadataWithConfig(meta_path, [](const AssetMetadata&) {
            return material_importer_detail::makeDefaultMaterialConfig();
        });
    }
};

} // namespace luna
