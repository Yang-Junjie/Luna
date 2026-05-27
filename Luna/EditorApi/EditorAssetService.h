#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace luna::editor {

struct AssetInfo {
    AssetHandle handle{0};
    AssetType type{AssetType::None};
    std::string label;
    std::string detail;
    bool exists{false};
    bool builtin{false};
    bool loading{false};
    bool memory_only{false};
    std::filesystem::path project_path;
    std::filesystem::path absolute_path;
};

struct AssetRefreshResult {
    bool success{false};
    bool project_loaded{false};
    uint64_t revision{0};
    std::string message;
    size_t discovered_assets{0};
    size_t imported_missing_assets{0};
    size_t loaded_existing_metadata{0};
    size_t rebuilt_metadata{0};
    size_t unsupported_files_skipped{0};
    size_t failed_assets{0};
    size_t missing_metadata_after_sync{0};
    size_t script_files_skipped_no_plugin{0};
    size_t script_files_skipped_unsupported_language{0};
    size_t generated_model_files{0};
    size_t generated_model_metadata{0};
    size_t generated_material_files{0};
    size_t generated_material_metadata{0};
    size_t generated_texture_metadata{0};
    size_t failed_generated_model_assets{0};
};

class AssetService {
public:
    virtual ~AssetService() = default;

    virtual AssetInfo describeAsset(AssetHandle handle) const = 0;
    virtual std::optional<AssetInfo> assetInfo(AssetHandle handle) const = 0;
    virtual std::optional<AssetInfo> assetInfoByPath(const std::filesystem::path& path) const = 0;
    virtual std::vector<AssetInfo> listAssets(AssetType type_filter, bool include_builtin) const = 0;
    virtual std::vector<AssetInfo> builtinAssets(AssetType type) const = 0;
    virtual bool assetExists(AssetHandle handle) const = 0;
    virtual bool assetPathExists(const std::filesystem::path& path) const = 0;
    virtual AssetHandle findAssetHandleByPath(const std::filesystem::path& path) const = 0;
    virtual std::optional<std::filesystem::path> assetsRootPath() const = 0;
    virtual std::optional<std::filesystem::path>
        resolveProjectAssetPath(const std::filesystem::path& project_relative_path) const = 0;
    virtual std::optional<std::filesystem::path>
        makeProjectRelativeAssetPath(const std::filesystem::path& path) const = 0;
    virtual AssetRefreshResult refreshAssets() = 0;
    virtual uint64_t assetRevision() const noexcept = 0;
    virtual bool isAssetLoading(AssetHandle handle) const = 0;
    virtual bool
        acceptsAssetType(AssetType type, const AssetType* accepted_types, std::size_t accepted_type_count) const = 0;
    virtual std::optional<std::size_t> meshSubmeshCount(AssetHandle mesh_handle) const = 0;
    virtual bool beginAssetDragDropSource(AssetHandle handle, std::string_view label = {}) = 0;

    bool acceptsAssetType(AssetType type, std::initializer_list<AssetType> accepted_types) const
    {
        return acceptsAssetType(type, accepted_types.begin(), accepted_types.size());
    }

    std::vector<AssetInfo> listAssets(AssetType type_filter = AssetType::None) const
    {
        return listAssets(type_filter, false);
    }
};

} // namespace luna::editor
