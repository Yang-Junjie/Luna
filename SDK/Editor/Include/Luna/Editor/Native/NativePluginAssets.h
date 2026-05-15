#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class PluginAssets final {
public:
    constexpr PluginAssets() noexcept = default;
    explicit constexpr PluginAssets(const LunaEditorPluginAssetApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool rootPath(char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->plugin_root_path != nullptr && out_path != nullptr &&
               api_->plugin_root_path(api_->api_user_data, out_path, out_path_size) != 0;
    }

    [[nodiscard]] bool assetRootPath(char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->asset_root_path != nullptr && out_path != nullptr &&
               api_->asset_root_path(api_->api_user_data, out_path, out_path_size) != 0;
    }

    [[nodiscard]] bool resolvePath(const char* relative_asset_path, char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->resolve_path != nullptr && out_path != nullptr &&
               api_->resolve_path(api_->api_user_data, relative_asset_path, out_path, out_path_size) != 0;
    }

    [[nodiscard]] bool exists(const char* relative_asset_path) const noexcept
    {
        return api_ != nullptr && api_->exists != nullptr &&
               api_->exists(api_->api_user_data, relative_asset_path) != 0;
    }

    [[nodiscard]] bool readText(const char* relative_asset_path,
                                char* out_text,
                                size_t out_text_size,
                                size_t* out_required_size = nullptr) const noexcept
    {
        return api_ != nullptr && api_->read_text != nullptr &&
               api_->read_text(api_->api_user_data,
                               relative_asset_path,
                               out_text,
                               out_text_size,
                               out_required_size) != 0;
    }

    [[nodiscard]] bool readBytes(const char* relative_asset_path,
                                 void* out_data,
                                 size_t out_data_size,
                                 size_t* out_required_size = nullptr) const noexcept
    {
        return api_ != nullptr && api_->read_bytes != nullptr &&
               api_->read_bytes(api_->api_user_data,
                                relative_asset_path,
                                out_data,
                                out_data_size,
                                out_required_size) != 0;
    }

    [[nodiscard]] bool texture(const char* relative_asset_path, TextureView* out_texture) const noexcept
    {
        return api_ != nullptr && api_->texture != nullptr && out_texture != nullptr &&
               api_->texture(api_->api_user_data, relative_asset_path, out_texture) != 0;
    }

    [[nodiscard]] const LunaEditorPluginAssetApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorPluginAssetApi* api_{};
};

} // namespace luna::editor::native
