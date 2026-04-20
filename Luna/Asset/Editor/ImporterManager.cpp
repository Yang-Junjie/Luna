#include "../AssetDatabase.h"
#include "Core/Log.h"
#include "ImporterManager.h"
#include "MaterialImporter.h"
#include "MeshImporter.h"
#include "Project/ProjectManager.h"
#include "TextureImporter.h"

#include <cctype>

#include <algorithm>
#include <filesystem>

namespace {

bool shouldRebuildMetadata(const luna::AssetMetadata& metadata,
                           const std::filesystem::path& asset_path,
                           const std::filesystem::path& project_root)
{
    if (!metadata.Handle.isValid() || metadata.Type == luna::AssetType::None || metadata.FilePath.empty()) {
        return true;
    }

    const std::filesystem::path expected_relative_path = luna::tools::makeRelative(asset_path, project_root);
    return metadata.FilePath.lexically_normal().generic_string() !=
           expected_relative_path.lexically_normal().generic_string();
}

std::filesystem::path getMetadataPathForAsset(const std::filesystem::path& asset_path)
{
    return std::filesystem::path(asset_path.string() + ".meta");
}

std::string toDisplayPath(const std::filesystem::path& path, const std::filesystem::path& project_root)
{
    const std::filesystem::path relative_path = luna::tools::makeRelative(path, project_root);
    if (!relative_path.empty() && !relative_path.is_absolute()) {
        return relative_path.generic_string();
    }

    return path.lexically_normal().generic_string();
}

} // namespace

namespace luna {

void ImporterManager::init()
{
    importers.clear();
    extensionToImporter.clear();

    auto register_importer = [](std::unique_ptr<Importer> importer) {
        for (const auto& ext : importer->getSupportedExtensions()) {
            extensionToImporter[importer_detail::normalizeExtension(ext)] = importer.get();
        }
        importers.push_back(std::move(importer));
    };

    register_importer(std::make_unique<MeshImporter>());
    register_importer(std::make_unique<MaterialImporter>());
    register_importer(std::make_unique<TextureImporter>());
}

void ImporterManager::import()
{
    (void) syncProjectAssets();
}

ImporterManager::ImportStats ImporterManager::syncProjectAssets()
{
    init();
    ImportStats stats{};

    const auto project_info = ProjectManager::instance().getProjectInfo();
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    if (!project_info || !project_root) {
        return stats;
    }

    const auto root = (*project_root / project_info->AssetsPath).lexically_normal();
    if (!std::filesystem::exists(root)) {
        return stats;
    }

    std::vector<std::filesystem::path> supported_assets;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto& path = entry.path();

        if (!entry.is_regular_file() || path.extension() == ".meta") {
            continue;
        }

        const std::string ext = importer_detail::normalizeExtension(path);

        if (!extensionToImporter.contains(ext)) {
            ++stats.unsupportedFilesSkipped;
            continue;
        }

        supported_assets.push_back(path);
        ++stats.discoveredAssets;
        auto* importer = extensionToImporter[ext];

        const std::filesystem::path meta_path = getMetadataPathForAsset(path);

        AssetMetadata metadata;
        bool should_serialize_metadata = false;

        if (std::filesystem::exists(meta_path)) {
            metadata = importer->deserializeMetadata(meta_path);
            if (shouldRebuildMetadata(metadata, path, *project_root)) {
                metadata = importer->import(path);
                should_serialize_metadata = true;
                ++stats.rebuiltMetadata;
                LUNA_CORE_WARN("Rebuilt invalid asset metadata for '{}'", toDisplayPath(path, *project_root));
            } else {
                ++stats.loadedExistingMetadata;
            }
        } else {
            metadata = importer->import(path);
            should_serialize_metadata = true;
            ++stats.importedMissingAssets;
            LUNA_CORE_INFO("Imported missing asset metadata for '{}'", toDisplayPath(path, *project_root));
        }

        if (should_serialize_metadata) {
            importer->serializeMetadata(metadata);
        }

        if (!std::filesystem::exists(meta_path)) {
            ++stats.failedAssets;
            LUNA_CORE_WARN("Failed to synchronize asset '{}': metadata file was not written",
                           toDisplayPath(path, *project_root));
            continue;
        }

        AssetDatabase::set(metadata.Handle, metadata);
    }

    for (const auto& supported_asset_path : supported_assets) {
        if (!std::filesystem::exists(getMetadataPathForAsset(supported_asset_path))) {
            ++stats.missingMetadataAfterSync;
            LUNA_CORE_ERROR("Supported asset is still missing metadata after sync: '{}'",
                            toDisplayPath(supported_asset_path, *project_root));
        }
    }

    LUNA_CORE_INFO(
        "Asset sync completed. discovered={}, imported_missing={}, loaded_existing={}, rebuilt={}, unsupported={}, "
        "failed={}, missing_after_sync={}",
        stats.discoveredAssets,
        stats.importedMissingAssets,
        stats.loadedExistingMetadata,
        stats.rebuiltMetadata,
        stats.unsupportedFilesSkipped,
        stats.failedAssets,
        stats.missingMetadataAfterSync);

    return stats;
}

Importer* ImporterManager::getImporter(std::string extension)
{
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (const auto it = extensionToImporter.find(extension); it != extensionToImporter.end()) {
        return it->second;
    }

    return nullptr;
}

} // namespace luna
