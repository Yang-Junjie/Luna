#pragma once

#include "Importer.h"

namespace luna::script_importer_detail {

inline YAML::Node makeDefaultScriptConfig()
{
    YAML::Node config(YAML::NodeType::Map);
    config["Language"] = "Lua";
    return config;
}

} // namespace luna::script_importer_detail

namespace luna {

class ScriptImporter final : public Importer {
public:
    AssetMetadata import(const std::filesystem::path& script_path) override
    {
        AssetMetadata metadata = importer_detail::makeAssetMetadata(script_path, AssetType::Script);
        metadata.SpecializedConfig = script_importer_detail::makeDefaultScriptConfig();
        return metadata;
    }

    std::vector<std::string> getSupportedExtensions() const override
    {
        return {".lua"};
    }

    void serializeMetadata(const AssetMetadata& metadata) override
    {
        importer_detail::serializeMetadataWithConfig(metadata, [](const AssetMetadata&) {
            return script_importer_detail::makeDefaultScriptConfig();
        });
    }

    AssetMetadata deserializeMetadata(const std::filesystem::path& meta_path) override
    {
        return importer_detail::deserializeMetadataWithConfig(meta_path, [](const AssetMetadata&) {
            return script_importer_detail::makeDefaultScriptConfig();
        });
    }
};

} // namespace luna
