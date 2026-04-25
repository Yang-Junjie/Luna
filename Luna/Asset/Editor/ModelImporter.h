#pragma once

#include "Importer.h"

namespace luna {

namespace model_importer_detail {

inline YAML::Node makeDefaultModelConfig()
{
    YAML::Node config(YAML::NodeType::Map);
    config["UseModelFileData"] = true;
    return config;
}

} // namespace model_importer_detail

class ModelImporter final : public Importer {
public:
    AssetMetadata import(const std::filesystem::path& model_path) override
    {
        AssetMetadata metadata = importer_detail::makeAssetMetadata(model_path, AssetType::Model);
        metadata.SpecializedConfig = model_importer_detail::makeDefaultModelConfig();
        return metadata;
    }

    std::vector<std::string> getSupportedExtensions() const override
    {
        return {".lmodel"};
    }

    void serializeMetadata(const AssetMetadata& metadata) override
    {
        importer_detail::serializeMetadataWithConfig(metadata, [](const AssetMetadata&) {
            return model_importer_detail::makeDefaultModelConfig();
        });
    }

    AssetMetadata deserializeMetadata(const std::filesystem::path& meta_path) override
    {
        return importer_detail::deserializeMetadataWithConfig(meta_path, [](const AssetMetadata&) {
            return model_importer_detail::makeDefaultModelConfig();
        });
    }
};

} // namespace luna
