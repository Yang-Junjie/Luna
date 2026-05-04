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
#include "ScriptImporter.h"
#include "Script/ScriptPluginManager.h"
#include "TextureImporter.h"

#include <cctype>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <string>
#include <unordered_set>
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

struct ResolvedScriptImportSupport {
    std::string language;
    std::string backend_name;
    std::vector<std::string> extensions;
    std::string status_message;
    bool available = false;
};

bool pathExistsNoThrow(const std::filesystem::path& path);

std::vector<std::string> normalizeExtensions(const std::vector<std::string>& extensions)
{
    std::vector<std::string> normalized_extensions;
    std::unordered_set<std::string> seen_extensions;

    for (const std::string& extension : extensions) {
        const std::string normalized_extension = luna::importer_detail::normalizeExtension(extension);
        if (normalized_extension.empty() || seen_extensions.contains(normalized_extension)) {
            continue;
        }

        seen_extensions.insert(normalized_extension);
        normalized_extensions.push_back(normalized_extension);
    }

    return normalized_extensions;
}

ResolvedScriptImportSupport resolveScriptImportSupport(bool log_failures)
{
    ResolvedScriptImportSupport support{};

    const auto project_root = luna::ProjectManager::instance().getProjectRootPath();
    luna::ScriptPluginManager::instance().refreshDiscoveredPlugins(project_root);

    const auto project_info = luna::ProjectManager::instance().getProjectInfo();
    const luna::ScriptPluginSelectionResult selection =
        luna::ScriptPluginManager::instance().resolveAndLoadProjectSelection(project_info ? &*project_info : nullptr);
    if (!selection.isResolved()) {
        support.status_message = selection.StatusMessage;
        if (log_failures && !support.status_message.empty()) {
            LUNA_CORE_ERROR("Script importing is disabled: {}", support.status_message);
        }
        return support;
    }

    if (selection.Candidate == nullptr) {
        support.status_message = "The resolved script selection does not reference a script plugin.";
        if (log_failures) {
            LUNA_CORE_ERROR("Script importing is disabled: {}", support.status_message);
        }
        return support;
    }

    support.language = selection.Candidate->Manifest.Language;
    support.backend_name = selection.BackendName;

    if (const luna::ScriptBackendDescriptor* backend =
            luna::ScriptPluginManager::instance().findBackend(selection.BackendName);
        backend != nullptr) {
        support.extensions = backend->supported_extensions;
    }

    if (support.extensions.empty()) {
        support.extensions = selection.Candidate->Manifest.SupportedExtensions;
    }

    support.extensions = normalizeExtensions(support.extensions);

    if (support.language.empty()) {
        support.status_message = "Selected script plugin '" + selection.Candidate->Manifest.PluginId +
                                 "' does not declare a script language.";
        if (log_failures) {
            LUNA_CORE_ERROR("Script importing is disabled: {}", support.status_message);
        }
        return support;
    }

    if (support.extensions.empty()) {
        support.status_message = "Selected script plugin '" + selection.Candidate->Manifest.PluginId +
                                 "' does not declare any supported script file extensions.";
        if (log_failures) {
            LUNA_CORE_ERROR("Script importing is disabled: {}", support.status_message);
        }
        return support;
    }

    support.available = true;
    return support;
}

std::unordered_set<std::string> collectManifestScriptExtensions()
{
    std::unordered_set<std::string> extensions;
    for (const luna::ScriptPluginCandidate& candidate :
         luna::ScriptPluginManager::instance().getDiscoveredPlugins()) {
        for (const std::string& extension : candidate.Manifest.SupportedExtensions) {
            const std::string normalized_extension = luna::importer_detail::normalizeExtension(extension);
            if (!normalized_extension.empty()) {
                extensions.insert(normalized_extension);
            }
        }
    }

    return extensions;
}

bool metadataDeclaresScriptAsset(const std::filesystem::path& meta_path)
{
    if (!pathExistsNoThrow(meta_path)) {
        return false;
    }

    const YAML::Node asset_node = luna::importer_detail::loadMetadataNode(meta_path);
    if (!asset_node) {
        return false;
    }

    luna::AssetMetadata metadata;
    luna::importer_detail::readCommonMetadata(asset_node, metadata);
    return metadata.Type == luna::AssetType::Script;
}

bool isKnownScriptFile(const std::filesystem::path& asset_path,
                       const std::filesystem::path& meta_path,
                       const std::unordered_set<std::string>& known_script_extensions)
{
    const std::string extension = luna::importer_detail::normalizeExtension(asset_path);
    return known_script_extensions.contains(extension) || metadataDeclaresScriptAsset(meta_path);
}

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

bool shouldRebuildScriptMetadata(const luna::AssetMetadata& metadata, const std::string& expected_language)
{
    if (metadata.Type != luna::AssetType::Script) {
        return false;
    }

    return metadata.GetConfig<std::string>("Language", "") != expected_language;
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

bool pathExistsNoThrow(const std::filesystem::path& path)
{
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        LUNA_CORE_WARN("Failed to check path '{}': {}", path.string(), ec.message());
        return false;
    }

    return exists;
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
                                           const std::filesystem::path& project_root,
                                           const std::string& expected_script_language)
{
    SupportedAssetResult result{};

    if (work_item.importer == nullptr) {
        result.failed = true;
        return result;
    }

    bool should_serialize_metadata = false;

    if (pathExistsNoThrow(work_item.metaPath)) {
        result.metadata = work_item.importer->deserializeMetadata(work_item.metaPath);
        if (shouldRebuildMetadata(result.metadata, work_item.assetPath, project_root) ||
            shouldRebuildScriptMetadata(result.metadata, expected_script_language)) {
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

    if (!pathExistsNoThrow(work_item.metaPath)) {
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
    const uint32_t worker_count = (std::max) (task_system.getWorkerThreadCount(), 1u);
    const uint32_t task_count =
        static_cast<uint32_t>((std::min) (work_item_count, static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
    const uint32_t target_chunk_count = (std::max) (worker_count * 4u, 1u);
    return (std::max) (1u, (task_count + target_chunk_count - 1u) / target_chunk_count);
}

} // namespace

namespace luna {

void ImporterManager::init()
{
    importers.clear();
    extensionToImporter.clear();

    ResolvedScriptImportSupport script_support = resolveScriptImportSupport(true);

    auto register_importer = [](std::unique_ptr<Importer> importer) {
        for (const auto& ext : importer->getSupportedExtensions()) {
            extensionToImporter[importer_detail::normalizeExtension(ext)] = importer.get();
        }
        importers.push_back(std::move(importer));
    };

    register_importer(std::make_unique<MeshImporter>());
    register_importer(std::make_unique<MaterialImporter>());
    register_importer(std::make_unique<ModelImporter>());
    if (script_support.available) {
        register_importer(
            std::make_unique<ScriptImporter>(std::move(script_support.language), std::move(script_support.extensions)));
    }
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

    const ResolvedScriptImportSupport script_support = resolveScriptImportSupport(false);
    const std::string expected_script_language = script_support.available ? script_support.language : std::string{};
    std::unordered_set<std::string> known_script_extensions = collectManifestScriptExtensions();
    for (const std::string& extension : script_support.extensions) {
        known_script_extensions.insert(importer_detail::normalizeExtension(extension));
    }

    const auto root = (*project_root / project_info->AssetsPath).lexically_normal();
    if (!pathExistsNoThrow(root)) {
        return stats;
    }

    std::vector<SupportedAssetWorkItem> supported_assets;

    std::error_code iterate_ec;
    std::filesystem::recursive_directory_iterator directory_it(
        root, std::filesystem::directory_options::skip_permission_denied, iterate_ec);
    if (iterate_ec) {
        LUNA_CORE_WARN("Failed to scan project assets directory '{}': {}", root.string(), iterate_ec.message());
        return stats;
    }

    for (const std::filesystem::recursive_directory_iterator directory_end; directory_it != directory_end;
         directory_it.increment(iterate_ec)) {
        if (iterate_ec) {
            LUNA_CORE_WARN(
                "Failed to advance asset directory scan under '{}': {}", root.string(), iterate_ec.message());
            iterate_ec.clear();
            continue;
        }

        const auto& entry = *directory_it;
        const auto& path = entry.path();

        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec) || entry_ec || path.extension() == ".meta") {
            continue;
        }

        const std::string ext = importer_detail::normalizeExtension(path);

        if (!extensionToImporter.contains(ext)) {
            if (isKnownScriptFile(path, getMetadataPathForAsset(path), known_script_extensions)) {
                if (!script_support.available) {
                    ++stats.scriptFilesSkippedNoPlugin;
                    LUNA_CORE_ERROR("Skipped script file '{}' because the project has no usable script plugin: {}",
                                    toDisplayPath(path, *project_root),
                                    script_support.status_message.empty() ? "unknown script plugin error"
                                                                          : script_support.status_message);
                } else {
                    ++stats.scriptFilesSkippedUnsupportedLanguage;
                    LUNA_CORE_WARN(
                        "Skipped script file '{}' because extension '{}' is not supported by the selected {} script "
                        "plugin '{}'",
                        toDisplayPath(path, *project_root),
                        ext,
                        script_support.language,
                        script_support.backend_name);
                }
                continue;
            }

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
        submit_desc.set_size = static_cast<uint32_t>(
            (std::min) (supported_assets.size(), static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
        submit_desc.min_range = computeParallelMinRange(*task_system, supported_assets.size());

        TaskHandle task = task_system->submitParallel(
            [&](enki::TaskSetPartition range, uint32_t) {
                for (uint32_t index = range.start; index < range.end; ++index) {
                    results[index] = processSupportedAsset(supported_assets[index], *project_root,
                                                           expected_script_language);
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
            results[index] = processSupportedAsset(supported_assets[index], *project_root, expected_script_language);
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
        if (!pathExistsNoThrow(supported_asset.metaPath)) {
            ++stats.missingMetadataAfterSync;
            LUNA_CORE_ERROR("Supported asset is still missing metadata after sync: '{}'",
                            toDisplayPath(supported_asset.assetPath, *project_root));
        }
    }

    LUNA_CORE_INFO(
        "Asset sync completed. discovered={}, imported_missing={}, loaded_existing={}, rebuilt={}, unsupported={}, "
        "script_skipped_no_plugin={}, script_skipped_unsupported_language={}, failed={}, missing_after_sync={}, "
        "generated_models={}, generated_model_meta={}, generated_materials={}, generated_material_meta={}, "
        "generated_texture_meta={}, generated_model_failures={}",
        stats.discoveredAssets,
        stats.importedMissingAssets,
        stats.loadedExistingMetadata,
        stats.rebuiltMetadata,
        stats.unsupportedFilesSkipped,
        stats.scriptFilesSkippedNoPlugin,
        stats.scriptFilesSkippedUnsupportedLanguage,
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
