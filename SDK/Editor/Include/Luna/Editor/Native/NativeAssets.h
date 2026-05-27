#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

#include <array>
#include <vector>

namespace luna::editor::native {

class Assets final {
public:
    constexpr Assets() noexcept = default;

    explicit constexpr Assets(const LunaEditorAssetApi* api) noexcept
        : api_(api)
    {}

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
    }

    [[nodiscard]] bool describe(AssetHandle handle, LunaEditorAssetInfo* out_info) const noexcept
    {
        return api_ != nullptr && api_->describe_asset != nullptr && out_info != nullptr &&
               api_->describe_asset(api_->api_user_data, handle, out_info) != 0;
    }

    [[nodiscard]] bool info(uint64_t handle, LunaEditorAssetInfo* out_info) const noexcept
    {
        return api_ != nullptr && api_->asset_info != nullptr && out_info != nullptr &&
               api_->asset_info(api_->api_user_data, handle, out_info) != 0;
    }

    [[nodiscard]] bool infoByPath(const char* path, LunaEditorAssetInfo* out_info) const noexcept
    {
        return api_ != nullptr && api_->asset_info_by_path != nullptr && out_info != nullptr &&
               api_->asset_info_by_path(api_->api_user_data, path, out_info) != 0;
    }

    [[nodiscard]] AssetInfo describe(AssetHandle handle) const
    {
        AssetInfo result{};
        queryAssetInfo(&LunaEditorAssetApi::describe_asset, handle, result);
        return result;
    }

    [[nodiscard]] AssetInfo info(AssetHandle handle) const
    {
        AssetInfo result{};
        queryAssetInfo(&LunaEditorAssetApi::asset_info, handle, result);
        return result;
    }

    [[nodiscard]] AssetInfo infoByPath(const char* path) const
    {
        AssetInfo result{};
        if (api_ == nullptr || api_->asset_info_by_path == nullptr || path == nullptr) {
            return result;
        }

        std::array<char, 256> label{};
        std::array<char, 256> detail{};
        std::array<char, 512> project_path{};
        std::array<char, 512> absolute_path{};
        LunaEditorAssetInfo native_info = makeNativeAssetInfo(label, detail, project_path, absolute_path);
        if (api_->asset_info_by_path(api_->api_user_data, path, &native_info) != 0) {
            result = fromNative(native_info);
        }
        return result;
    }

    [[nodiscard]] size_t count(LunaEditorAssetType type_filter = LunaEditorAssetType_None,
                               bool include_builtin = true) const noexcept
    {
        if (api_ != nullptr && api_->list_assets != nullptr) {
            return api_->list_assets(api_->api_user_data, type_filter, include_builtin ? 1 : 0, nullptr, nullptr);
        }
        return 0u;
    }

    [[nodiscard]] std::vector<AssetInfo> list(LunaEditorAssetType type_filter = LunaEditorAssetType_None,
                                              bool include_builtin = true) const
    {
        std::vector<AssetInfo> result;
        if (api_ == nullptr || api_->list_assets == nullptr) {
            return result;
        }

        result.reserve(count(type_filter, include_builtin));
        api_->list_assets(api_->api_user_data, type_filter, include_builtin ? 1 : 0, &result, &appendAssetInfo);
        return result;
    }

    [[nodiscard]] bool exists(uint64_t handle) const noexcept
    {
        return api_ != nullptr && api_->asset_exists != nullptr && api_->asset_exists(api_->api_user_data, handle) != 0;
    }

    [[nodiscard]] bool pathExists(const char* path) const noexcept
    {
        return api_ != nullptr && api_->asset_path_exists != nullptr &&
               api_->asset_path_exists(api_->api_user_data, path) != 0;
    }

    [[nodiscard]] uint64_t findHandleByPath(const char* path) const noexcept
    {
        if (api_ != nullptr && api_->find_asset_handle_by_path != nullptr) {
            return api_->find_asset_handle_by_path(api_->api_user_data, path);
        }
        return 0u;
    }

    [[nodiscard]] bool rootPath(char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->assets_root_path != nullptr && out_path != nullptr &&
               api_->assets_root_path(api_->api_user_data, out_path, out_path_size) != 0;
    }

    [[nodiscard]] std::string rootPath() const
    {
        return readPath(&LunaEditorAssetApi::assets_root_path);
    }

    [[nodiscard]] bool
        resolveProjectPath(const char* project_relative_path, char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->resolve_project_asset_path != nullptr && project_relative_path != nullptr &&
               out_path != nullptr &&
               api_->resolve_project_asset_path(api_->api_user_data, project_relative_path, out_path, out_path_size) !=
                   0;
    }

    [[nodiscard]] std::string resolveProjectPath(const char* project_relative_path) const
    {
        return readPath(&LunaEditorAssetApi::resolve_project_asset_path, project_relative_path);
    }

    [[nodiscard]] bool makeProjectRelativePath(const char* path, char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->make_project_relative_asset_path != nullptr && path != nullptr &&
               out_path != nullptr &&
               api_->make_project_relative_asset_path(api_->api_user_data, path, out_path, out_path_size) != 0;
    }

    [[nodiscard]] std::string makeProjectRelativePath(const char* path) const
    {
        return readPath(&LunaEditorAssetApi::make_project_relative_asset_path, path);
    }

    [[nodiscard]] bool refresh(LunaEditorAssetRefreshResult* out_result = nullptr) const noexcept
    {
        return api_ != nullptr && api_->refresh_assets != nullptr &&
               api_->refresh_assets(api_->api_user_data, out_result) != 0;
    }

    [[nodiscard]] uint64_t revision() const noexcept
    {
        if (api_ != nullptr && api_->asset_revision != nullptr) {
            return api_->asset_revision(api_->api_user_data);
        }
        return 0u;
    }

    [[nodiscard]] bool loading(uint64_t handle) const noexcept
    {
        return api_ != nullptr && api_->is_asset_loading != nullptr &&
               api_->is_asset_loading(api_->api_user_data, handle) != 0;
    }

    [[nodiscard]] AssetRefreshResult refreshDetailed() const
    {
        AssetRefreshResult result{};
        std::array<char, 512> message{};
        LunaEditorAssetRefreshResult native_result{};
        native_result.struct_size = sizeof(LunaEditorAssetRefreshResult);
        native_result.api_version = LUNA_EDITOR_ASSET_REFRESH_RESULT_API_VERSION;
        native_result.message = message.data();
        native_result.message_size = message.size();
        result.success = refresh(&native_result);
        result.project_loaded = native_result.project_loaded != 0;
        result.revision = native_result.revision;
        result.message = native_result.message != nullptr ? native_result.message : "";
        result.discovered_assets = native_result.discovered_assets;
        result.imported_missing_assets = native_result.imported_missing_assets;
        result.loaded_existing_metadata = native_result.loaded_existing_metadata;
        result.rebuilt_metadata = native_result.rebuilt_metadata;
        result.unsupported_files_skipped = native_result.unsupported_files_skipped;
        result.failed_assets = native_result.failed_assets;
        result.missing_metadata_after_sync = native_result.missing_metadata_after_sync;
        result.script_files_skipped_no_plugin = native_result.script_files_skipped_no_plugin;
        result.script_files_skipped_unsupported_language = native_result.script_files_skipped_unsupported_language;
        result.generated_model_files = native_result.generated_model_files;
        result.generated_model_metadata = native_result.generated_model_metadata;
        result.generated_material_files = native_result.generated_material_files;
        result.generated_material_metadata = native_result.generated_material_metadata;
        result.generated_texture_metadata = native_result.generated_texture_metadata;
        result.failed_generated_model_assets = native_result.failed_generated_model_assets;
        return result;
    }

    [[nodiscard]] bool
        acceptsType(uint32_t type, const uint32_t* accepted_types, size_t accepted_type_count) const noexcept
    {
        return api_ != nullptr && api_->accepts_asset_type != nullptr &&
               api_->accepts_asset_type(api_->api_user_data, type, accepted_types, accepted_type_count) != 0;
    }

    [[nodiscard]] bool meshSubmeshCount(AssetHandle mesh_handle, size_t* out_count) const noexcept
    {
        return api_ != nullptr && api_->mesh_submesh_count != nullptr && out_count != nullptr &&
               api_->mesh_submesh_count(api_->api_user_data, mesh_handle, out_count) != 0;
    }

    [[nodiscard]] size_t meshSubmeshCount(AssetHandle mesh_handle) const noexcept
    {
        size_t count_value = 0u;
        (void) meshSubmeshCount(mesh_handle, &count_value);
        return count_value;
    }

    [[nodiscard]] bool beginDragDropSource(AssetHandle handle, const char* label = nullptr) const noexcept
    {
        return api_ != nullptr && api_->begin_asset_drag_drop_source != nullptr &&
               api_->begin_asset_drag_drop_source(api_->api_user_data, handle, label) != 0;
    }

    [[nodiscard]] const LunaEditorAssetApi* native() const noexcept
    {
        return api_;
    }

private:
    using AssetQueryFn = int (*)(void*, uint64_t, LunaEditorAssetInfo*);
    using AssetQueryMember = AssetQueryFn LunaEditorAssetApi::*;
    using PathReadFn = int (*)(void*, char*, size_t);
    using PathReadMember = PathReadFn LunaEditorAssetApi::*;
    using PathConvertFn = int (*)(void*, const char*, char*, size_t);
    using PathConvertMember = PathConvertFn LunaEditorAssetApi::*;

    template <size_t LabelSize, size_t DetailSize, size_t ProjectPathSize, size_t AbsolutePathSize>
    [[nodiscard]] static LunaEditorAssetInfo
        makeNativeAssetInfo(std::array<char, LabelSize>& label,
                            std::array<char, DetailSize>& detail,
                            std::array<char, ProjectPathSize>& project_path,
                            std::array<char, AbsolutePathSize>& absolute_path) noexcept
    {
        LunaEditorAssetInfo info{};
        info.struct_size = sizeof(LunaEditorAssetInfo);
        info.api_version = LUNA_EDITOR_ASSET_INFO_API_VERSION;
        info.label = label.data();
        info.label_size = label.size();
        info.detail = detail.data();
        info.detail_size = detail.size();
        info.project_path = project_path.data();
        info.project_path_size = project_path.size();
        info.absolute_path = absolute_path.data();
        info.absolute_path_size = absolute_path.size();
        return info;
    }

    [[nodiscard]] static AssetInfo fromNative(const LunaEditorAssetInfo& info)
    {
        return AssetInfo{
            .handle = info.handle,
            .type = static_cast<LunaEditorAssetType>(info.type),
            .exists = info.exists != 0,
            .builtin = info.builtin != 0,
            .loading = info.loading != 0,
            .memory_only = info.memory_only != 0,
            .label = info.label != nullptr ? info.label : "",
            .detail = info.detail != nullptr ? info.detail : "",
            .project_path = info.project_path != nullptr ? info.project_path : "",
            .absolute_path = info.absolute_path != nullptr ? info.absolute_path : "",
        };
    }

    bool queryAssetInfo(AssetQueryMember query, AssetHandle handle, AssetInfo& out_info) const
    {
        if (api_ == nullptr || query == nullptr || (api_->*query) == nullptr) {
            return false;
        }

        std::array<char, 256> label{};
        std::array<char, 256> detail{};
        std::array<char, 512> project_path{};
        std::array<char, 512> absolute_path{};
        LunaEditorAssetInfo native_info = makeNativeAssetInfo(label, detail, project_path, absolute_path);
        if ((api_->*query)(api_->api_user_data, handle, &native_info) == 0) {
            return false;
        }

        out_info = fromNative(native_info);
        return true;
    }

    [[nodiscard]] std::string readPath(PathReadMember read_fn) const
    {
        if (api_ == nullptr || read_fn == nullptr || (api_->*read_fn) == nullptr) {
            return {};
        }

        std::array<char, 1'024> buffer{};
        if ((api_->*read_fn)(api_->api_user_data, buffer.data(), buffer.size()) == 0) {
            return {};
        }
        return buffer.data();
    }

    [[nodiscard]] std::string readPath(PathConvertMember read_fn, const char* path) const
    {
        if (api_ == nullptr || read_fn == nullptr || (api_->*read_fn) == nullptr || path == nullptr) {
            return {};
        }

        std::array<char, 1'024> buffer{};
        if ((api_->*read_fn)(api_->api_user_data, path, buffer.data(), buffer.size()) == 0) {
            return {};
        }
        return buffer.data();
    }

    static int appendAssetInfo(void* user_data, const LunaEditorAssetInfo* asset_info)
    {
        auto* result = static_cast<std::vector<AssetInfo>*>(user_data);
        if (result == nullptr || asset_info == nullptr) {
            return 0;
        }

        result->push_back(fromNative(*asset_info));
        return 1;
    }

    const LunaEditorAssetApi* api_{};
};

} // namespace luna::editor::native
