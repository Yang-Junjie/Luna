#pragma once
#include "Asset/AssetMetadata.h"
#include "Asset/AssetTypes.h"
#include "Core/FileTool.h"
#include "Project/ProjectManager.h"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace luna {
class Importer {
public:
    virtual ~Importer() = default;
    virtual AssetMetadata import(const std::filesystem::path& assetPath) = 0;
    virtual std::vector<std::string> getSupportedExtensions() const = 0;
    virtual void serializeMetadata(const AssetMetadata& metadata) = 0;
    virtual AssetMetadata deserializeMetadata(const std::filesystem::path& metaPath) = 0;
};

namespace importer_detail {

inline AssetMetadata makeAssetMetadata(const std::filesystem::path& asset_path, AssetType type)
{
    AssetMetadata metadata;
    metadata.Name = asset_path.stem().string();
    metadata.Handle = UUID();
    if (static_cast<uint64_t>(metadata.Handle) == 0) {
        metadata.Handle = UUID(1);
    }
    metadata.Type = type;
    metadata.FilePath =
        luna::tools::makeRelative(asset_path, ProjectManager::instance().getProjectRootPath().value_or(asset_path.root_path()));
    return metadata;
}

inline std::filesystem::path getMetadataPath(const AssetMetadata& metadata)
{
    const auto project_root = ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    return project_root / (metadata.FilePath.string() + ".meta");
}

inline std::filesystem::path getMetadataPath(const std::filesystem::path& asset_path)
{
    return std::filesystem::path(asset_path.string() + ".meta");
}

inline void beginMetadataFile(YAML::Emitter& out)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Asset" << YAML::Value << YAML::BeginMap;
}

inline void writeCommonMetadata(YAML::Emitter& out, const AssetMetadata& metadata)
{
    out << YAML::Key << "Name" << YAML::Value << metadata.Name;
    out << YAML::Key << "Handle" << YAML::Value << static_cast<uint64_t>(metadata.Handle);
    out << YAML::Key << "Type" << YAML::Value << AssetUtils::AssetTypeToString(metadata.Type);
    out << YAML::Key << "FilePath" << YAML::Value << metadata.FilePath.generic_string();
    if (metadata.MemoryOnly) {
        out << YAML::Key << "MemoryOnly" << YAML::Value << metadata.MemoryOnly;
    }
}

inline void endMetadataFile(YAML::Emitter& out)
{
    out << YAML::EndMap;
    out << YAML::EndMap;
}

inline void writeMetadataFile(const YAML::Emitter& out, const std::filesystem::path& meta_path)
{
    std::ofstream fout(meta_path);
    fout << out.c_str();
}

inline YAML::Node loadMetadataNode(const std::filesystem::path& meta_path)
{
    const YAML::Node data = YAML::LoadFile(meta_path.string());
    return data["Asset"];
}

inline void readCommonMetadata(const YAML::Node& asset_node, AssetMetadata& metadata)
{
    if (!asset_node) {
        return;
    }

    if (asset_node["Name"]) {
        metadata.Name = asset_node["Name"].as<std::string>();
    }
    if (asset_node["Handle"]) {
        metadata.Handle = AssetHandle(asset_node["Handle"].as<uint64_t>());
    }
    if (asset_node["Type"]) {
        metadata.Type = AssetUtils::StringToAssetType(asset_node["Type"].as<std::string>());
    }
    if (asset_node["FilePath"]) {
        metadata.FilePath = asset_node["FilePath"].as<std::string>();
    }
    if (asset_node["MemoryOnly"]) {
        metadata.MemoryOnly = asset_node["MemoryOnly"].as<bool>();
    }
}

template <typename DefaultConfigFactory>
inline YAML::Node resolveConfig(const AssetMetadata& metadata, DefaultConfigFactory&& make_default_config)
{
    if (metadata.SpecializedConfig && metadata.SpecializedConfig.IsDefined()) {
        return metadata.SpecializedConfig;
    }
    return make_default_config(metadata);
}

template <typename DefaultConfigFactory>
inline void serializeMetadataWithConfig(const AssetMetadata& metadata, DefaultConfigFactory&& make_default_config)
{
    YAML::Emitter out;
    beginMetadataFile(out);
    writeCommonMetadata(out, metadata);
    out << YAML::Key << "Config" << YAML::Value << resolveConfig(metadata, std::forward<DefaultConfigFactory>(make_default_config));
    endMetadataFile(out);
    writeMetadataFile(out, getMetadataPath(metadata));
}

template <typename DefaultConfigFactory>
inline AssetMetadata deserializeMetadataWithConfig(const std::filesystem::path& meta_path,
                                                   DefaultConfigFactory&& make_default_config)
{
    AssetMetadata metadata;
    const YAML::Node asset_node = loadMetadataNode(meta_path);
    readCommonMetadata(asset_node, metadata);
    metadata.SpecializedConfig =
        asset_node && asset_node["Config"] ? asset_node["Config"] : make_default_config(metadata);
    return metadata;
}

inline std::string normalizeExtension(std::filesystem::path path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

} // namespace importer_detail
} // namespace luna
