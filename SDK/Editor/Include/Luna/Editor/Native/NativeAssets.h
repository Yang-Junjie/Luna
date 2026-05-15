#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Assets final {
public:
    constexpr Assets() noexcept = default;
    explicit constexpr Assets(const LunaEditorAssetApi* api) noexcept
        : api_(api)
    {
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

    [[nodiscard]] bool exists(uint64_t handle) const noexcept
    {
        return api_ != nullptr && api_->asset_exists != nullptr &&
               api_->asset_exists(api_->api_user_data, handle) != 0;
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

    [[nodiscard]] const LunaEditorAssetApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorAssetApi* api_{};
};

} // namespace luna::editor::native
