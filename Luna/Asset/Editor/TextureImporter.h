#pragma once

#include "Importer.h"

namespace luna {

namespace texture_importer_detail {

inline bool shouldDefaultToSrgb(const std::filesystem::path& texture_path)
{
    const std::string stem = texture_path.stem().string();
    std::string lower = stem;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lower.find("normal") != std::string::npos || lower.find("rough") != std::string::npos ||
        lower.find("metal") != std::string::npos || lower.find("orm") != std::string::npos ||
        lower.find("ao") != std::string::npos || lower.find("occlusion") != std::string::npos ||
        lower.find("mask") != std::string::npos) {
        return false;
    }

    return texture_path.extension() != ".hdr";
}

inline YAML::Node makeDefaultTextureConfig(const std::filesystem::path& texture_path = {})
{
    YAML::Node config(YAML::NodeType::Map);
    config["GenerateMipmaps"] = true;
    config["SRGB"] = texture_path.empty() ? true : shouldDefaultToSrgb(texture_path);
    config["MinFilter"] = "Linear";
    config["MagFilter"] = "Linear";
    config["MipFilter"] = "Linear";
    config["WrapU"] = "Repeat";
    config["WrapV"] = "Repeat";
    config["WrapW"] = "Repeat";
    return config;
}

} // namespace texture_importer_detail

class TextureImporter final : public Importer {
public:
    AssetMetadata import(const std::filesystem::path& texture_path) override
    {
        AssetMetadata metadata = importer_detail::makeAssetMetadata(texture_path, AssetType::Texture);
        metadata.SpecializedConfig = texture_importer_detail::makeDefaultTextureConfig(texture_path);
        return metadata;
    }

    std::vector<std::string> getSupportedExtensions() const override
    {
        return {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".dds", ".ktx", ".ktx2"};
    }

    void serializeMetadata(const AssetMetadata& metadata) override
    {
        importer_detail::serializeMetadataWithConfig(metadata, [](const AssetMetadata& config_metadata) {
            return texture_importer_detail::makeDefaultTextureConfig(config_metadata.FilePath);
        });
    }

    AssetMetadata deserializeMetadata(const std::filesystem::path& meta_path) override
    {
        return importer_detail::deserializeMetadataWithConfig(meta_path, [](const AssetMetadata& config_metadata) {
            return texture_importer_detail::makeDefaultTextureConfig(config_metadata.FilePath);
        });
    }
};

} // namespace luna
