#include "../AssetDatabase.h"
#include "Core/Log.h"
#include "FbxModelAssetGenerator.h"
#include "GltfModelAssetGenerator.h"
#include "ImporterManager.h"
#include "JobSystem/TaskSystem.h"
#include "MaterialImporter.h"
#include "MeshImporter.h"
#include "ModelImporter.h"
#include "Project/ProjectManager.h"
#include "TextureImporter.h"

#include <cctype>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <vector>

namespace {

struct SupportedAssetWorkItem {
    std::filesystem::path assetPath;
    std::filesystem::path metaPath;
    luna::Importer* importer = nullptr;
};

struct SupportedAssetResult {
    luna::AssetMetadata metadata;
    bool shouldCommit = false;
    bool importedMissingAsset = false;
    bool loadedExistingMetadata = false;
    bool rebuiltMetadata = false;
    bool failed = false;
};

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

bool isGltfModelPath(const std::filesystem::path& path)
{
    const std::string extension = luna::importer_detail::normalizeExtension(path);
    return extension == ".gltf" || extension == ".glb";
}

bool isFbxModelPath(const std::filesystem::path& path)
{
    const std::string extension = luna::importer_detail::normalizeExtension(path);
    return extension == ".fbx";
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

SupportedAssetResult processSupportedAsset(const SupportedAssetWorkItem& work_item,
                                           const std::filesystem::path& project_root)
{
    SupportedAssetResult result{};

    if (work_item.importer == nullptr) {
        result.failed = true;
        return result;
    }

    bool should_serialize_metadata = false;

    if (std::filesystem::exists(work_item.metaPath)) {
        result.metadata = work_item.importer->deserializeMetadata(work_item.metaPath);
        if (shouldRebuildMetadata(result.metadata, work_item.assetPath, project_root)) {
            result.metadata = work_item.importer->import(work_item.assetPath);
            should_serialize_metadata = true;
            result.rebuiltMetadata = true;
            LUNA_CORE_WARN("Rebuilt invalid asset metadata for '{}'", toDisplayPath(work_item.assetPath, project_root));
        } else {
            result.loadedExistingMetadata = true;
        }
    } else {
        result.metadata = work_item.importer->import(work_item.assetPath);
        should_serialize_metadata = true;
        result.importedMissingAsset = true;
        LUNA_CORE_INFO("Imported missing asset metadata for '{}'", toDisplayPath(work_item.assetPath, project_root));
    }

    if (should_serialize_metadata) {
        work_item.importer->serializeMetadata(result.metadata);
    }

    if (!std::filesystem::exists(work_item.metaPath)) {
        result.failed = true;
        LUNA_CORE_WARN("Failed to synchronize asset '{}': metadata file was not written",
                       toDisplayPath(work_item.assetPath, project_root));
        return result;
    }

    result.shouldCommit = true;
    return result;
}

uint32_t computeParallelMinRange(const luna::TaskSystem& task_system, size_t work_item_count)
{
    const uint32_t worker_count = (std::max)(task_system.getWorkerThreadCount(), 1u);
    const uint32_t task_count = static_cast<uint32_t>((std::min)(work_item_count, static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
    const uint32_t target_chunk_count = (std::max)(worker_count * 4u, 1u);
    return (std::max)(1u, (task_count + target_chunk_count - 1u) / target_chunk_count);
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
    register_importer(std::make_unique<ModelImporter>());
    register_importer(std::make_unique<TextureImporter>());
}

void ImporterManager::import()
{
    (void) syncProjectAssets();
}

ImporterManager::ImportStats ImporterManager::syncProjectAssets(TaskSystem* task_system)
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

    std::vector<SupportedAssetWorkItem> supported_assets;

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

        supported_assets.push_back(SupportedAssetWorkItem{
            .assetPath = path,
            .metaPath = getMetadataPathForAsset(path),
            .importer = extensionToImporter[ext],
        });
        ++stats.discoveredAssets;
    }

    std::vector<SupportedAssetResult> results(supported_assets.size());

    bool used_parallel_sync = false;
    if (task_system != nullptr && supported_assets.size() > 1 && task_system->getWorkerThreadCount() > 1) {
        TaskSubmitDesc submit_desc{};
        submit_desc.priority = enki::TASK_PRIORITY_MED;
        submit_desc.set_size =
            static_cast<uint32_t>((std::min)(supported_assets.size(), static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
        submit_desc.min_range = computeParallelMinRange(*task_system, supported_assets.size());

        TaskHandle task = task_system->submitParallel(
            [&](enki::TaskSetPartition range, uint32_t) {
                for (uint32_t index = range.start; index < range.end; ++index) {
                    results[index] = processSupportedAsset(supported_assets[index], *project_root);
                }
            },
            submit_desc);

        if (task.isValid()) {
            task.wait(*task_system);
            used_parallel_sync = true;
        }
    }

    if (!used_parallel_sync) {
        for (size_t index = 0; index < supported_assets.size(); ++index) {
            results[index] = processSupportedAsset(supported_assets[index], *project_root);
        }
    }

    for (const SupportedAssetResult& result : results) {
        if (result.importedMissingAsset) {
            ++stats.importedMissingAssets;
        }
        if (result.loadedExistingMetadata) {
            ++stats.loadedExistingMetadata;
        }
        if (result.rebuiltMetadata) {
            ++stats.rebuiltMetadata;
        }
        if (result.failed) {
            ++stats.failedAssets;
            continue;
        }
        if (result.shouldCommit) {
            AssetDatabase::set(result.metadata.Handle, result.metadata);
        }
    }

    for (const SupportedAssetWorkItem& supported_asset : supported_assets) {
        const bool is_gltf_model = isGltfModelPath(supported_asset.assetPath);
        const bool is_fbx_model = isFbxModelPath(supported_asset.assetPath);
        if (!is_gltf_model && !is_fbx_model) {
            continue;
        }

        const AssetHandle mesh_handle = AssetDatabase::findHandleByFilePath(supported_asset.assetPath);
        if (!mesh_handle.isValid() || !AssetDatabase::exists(mesh_handle)) {
            ++stats.failedGeneratedModelAssets;
            continue;
        }

        const AssetMetadata& mesh_metadata = AssetDatabase::getAssetMetadata(mesh_handle);
        if (mesh_metadata.Type != AssetType::Mesh) {
            ++stats.failedGeneratedModelAssets;
            continue;
        }

        size_t created_material_files = 0;
        size_t created_material_metadata = 0;
        size_t created_texture_metadata = 0;
        bool created_model_file = false;
        bool created_model_metadata = false;
        bool generated_success = false;

        if (is_gltf_model) {
            const GltfModelAssetGenerator::GenerateResult generate_result =
                GltfModelAssetGenerator::generateCompanionAssets(supported_asset.assetPath, mesh_metadata);
            generated_success = generate_result.Success;
            created_model_file = generate_result.CreatedModelFile;
            created_model_metadata = generate_result.CreatedModelMetadata;
            created_material_files = generate_result.CreatedMaterialFiles;
            created_material_metadata = generate_result.CreatedMaterialMetadata;
            created_texture_metadata = generate_result.CreatedTextureMetadata;
        } else {
            const FbxModelAssetGenerator::GenerateResult generate_result =
                FbxModelAssetGenerator::generateCompanionAssets(supported_asset.assetPath, mesh_metadata);
            generated_success = generate_result.Success;
            created_model_file = generate_result.CreatedModelFile;
            created_model_metadata = generate_result.CreatedModelMetadata;
            created_material_files = generate_result.CreatedMaterialFiles;
            created_material_metadata = generate_result.CreatedMaterialMetadata;
            created_texture_metadata = generate_result.CreatedTextureMetadata;
        }

        if (!generated_success) {
            ++stats.failedGeneratedModelAssets;
            continue;
        }

        stats.generatedModelFiles += created_model_file ? 1 : 0;
        stats.generatedModelMetadata += created_model_metadata ? 1 : 0;
        stats.generatedMaterialFiles += created_material_files;
        stats.generatedMaterialMetadata += created_material_metadata;
        stats.generatedTextureMetadata += created_texture_metadata;
    }

    for (const SupportedAssetWorkItem& supported_asset : supported_assets) {
        if (!std::filesystem::exists(supported_asset.metaPath)) {
            ++stats.missingMetadataAfterSync;
            LUNA_CORE_ERROR("Supported asset is still missing metadata after sync: '{}'",
                            toDisplayPath(supported_asset.assetPath, *project_root));
        }
    }

    LUNA_CORE_INFO(
        "Asset sync completed. discovered={}, imported_missing={}, loaded_existing={}, rebuilt={}, unsupported={}, "
        "failed={}, missing_after_sync={}, generated_models={}, generated_model_meta={}, generated_materials={}, "
        "generated_material_meta={}, generated_texture_meta={}, generated_model_failures={}",
        stats.discoveredAssets,
        stats.importedMissingAssets,
        stats.loadedExistingMetadata,
        stats.rebuiltMetadata,
        stats.unsupportedFilesSkipped,
        stats.failedAssets,
        stats.missingMetadataAfterSync,
        stats.generatedModelFiles,
        stats.generatedModelMetadata,
        stats.generatedMaterialFiles,
        stats.generatedMaterialMetadata,
        stats.generatedTextureMetadata,
        stats.failedGeneratedModelAssets);

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
