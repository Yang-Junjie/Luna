#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace luna::editor::native {

class PluginAssets final {
public:
    constexpr PluginAssets() noexcept = default;
    explicit constexpr PluginAssets(const LunaEditorPluginAssetApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
    }

    [[nodiscard]] bool rootPath(char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->plugin_root_path != nullptr && out_path != nullptr &&
               api_->plugin_root_path(api_->api_user_data, out_path, out_path_size) != 0;
    }

    [[nodiscard]] std::string rootPath() const
    {
        return readPath(&LunaEditorPluginAssetApi::plugin_root_path);
    }

    [[nodiscard]] bool assetRootPath(char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->asset_root_path != nullptr && out_path != nullptr &&
               api_->asset_root_path(api_->api_user_data, out_path, out_path_size) != 0;
    }

    [[nodiscard]] std::string assetRootPath() const
    {
        return readPath(&LunaEditorPluginAssetApi::asset_root_path);
    }

    [[nodiscard]] bool resolvePath(const char* relative_asset_path, char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->resolve_path != nullptr && out_path != nullptr &&
               api_->resolve_path(api_->api_user_data, relative_asset_path, out_path, out_path_size) != 0;
    }

    [[nodiscard]] std::string resolvePath(const char* relative_asset_path) const
    {
        if (api_ == nullptr || api_->resolve_path == nullptr || relative_asset_path == nullptr) {
            return {};
        }

        std::array<char, 1024> buffer{};
        if (api_->resolve_path(api_->api_user_data, relative_asset_path, buffer.data(), buffer.size()) == 0) {
            return {};
        }
        return buffer.data();
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

    [[nodiscard]] std::string readText(const char* relative_asset_path) const
    {
        size_t required_size = 0u;
        if (!readText(relative_asset_path, nullptr, 0u, &required_size) || required_size == 0u) {
            return {};
        }

        std::string text(required_size, '\0');
        if (!readText(relative_asset_path, text.data(), text.size(), &required_size)) {
            if (!text.empty()) {
                text.pop_back();
            }
            return text;
        }
        if (!text.empty() && text.back() == '\0') {
            text.pop_back();
        }
        return text;
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

    [[nodiscard]] std::vector<uint8_t> readBytes(const char* relative_asset_path) const
    {
        size_t required_size = 0u;
        if (!readBytes(relative_asset_path, nullptr, 0u, &required_size)) {
            return {};
        }

        std::vector<uint8_t> bytes(required_size);
        if (required_size == 0u) {
            return bytes;
        }
        if (!readBytes(relative_asset_path, bytes.data(), bytes.size(), &required_size)) {
            bytes.resize((std::min)(bytes.size(), required_size));
        }
        return bytes;
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
    using PathReadFn = int (*)(void*, char*, size_t);
    using PathReadMember = PathReadFn LunaEditorPluginAssetApi::*;

    [[nodiscard]] std::string readPath(PathReadMember read_fn) const
    {
        if (api_ == nullptr || read_fn == nullptr || (api_->*read_fn) == nullptr) {
            return {};
        }

        std::array<char, 1024> buffer{};
        if ((api_->*read_fn)(api_->api_user_data, buffer.data(), buffer.size()) == 0) {
            return {};
        }
        return buffer.data();
    }

    const LunaEditorPluginAssetApi* api_{};
};

} // namespace luna::editor::native
