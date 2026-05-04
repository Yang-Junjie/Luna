#pragma once

#include "Importer.h"

#include <utility>

namespace luna::script_importer_detail {

inline YAML::Node makeDefaultScriptConfig(const std::string& language)
{
    YAML::Node config(YAML::NodeType::Map);
    if (!language.empty()) {
        config["Language"] = language;
    }
    return config;
}

} // namespace luna::script_importer_detail

namespace luna {

class ScriptImporter final : public Importer {
public:
    ScriptImporter() = default;

    ScriptImporter(std::string language, std::vector<std::string> supported_extensions)
        : m_language(std::move(language)),
          m_supported_extensions(std::move(supported_extensions))
    {}

    AssetMetadata import(const std::filesystem::path& script_path) override
    {
        AssetMetadata metadata = importer_detail::makeAssetMetadata(script_path, AssetType::Script);
        metadata.SpecializedConfig = script_importer_detail::makeDefaultScriptConfig(m_language);
        return metadata;
    }

    std::vector<std::string> getSupportedExtensions() const override
    {
        return m_supported_extensions;
    }

    void serializeMetadata(const AssetMetadata& metadata) override
    {
        importer_detail::serializeMetadataWithConfig(metadata, [this](const AssetMetadata&) {
            return script_importer_detail::makeDefaultScriptConfig(m_language);
        });
    }

    AssetMetadata deserializeMetadata(const std::filesystem::path& meta_path) override
    {
        return importer_detail::deserializeMetadataWithConfig(meta_path, [this](const AssetMetadata&) {
            return script_importer_detail::makeDefaultScriptConfig(m_language);
        });
    }

    const std::string& language() const noexcept
    {
        return m_language;
    }

    const std::vector<std::string>& supportedExtensions() const noexcept
    {
        return m_supported_extensions;
    }

private:
    std::string m_language;
    std::vector<std::string> m_supported_extensions;
};

} // namespace luna
