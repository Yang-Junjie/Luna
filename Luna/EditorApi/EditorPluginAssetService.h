#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace luna::editor {

struct PluginAssetBytes {
    std::vector<uint8_t> data;

    [[nodiscard]] bool empty() const noexcept
    {
        return data.empty();
    }
};

class PluginAssetService {
public:
    virtual ~PluginAssetService() = default;

    [[nodiscard]] virtual std::optional<std::filesystem::path> pluginRootPath(std::string_view plugin_id) const = 0;
    [[nodiscard]] virtual std::optional<std::filesystem::path> assetRootPath(std::string_view plugin_id) const = 0;
    [[nodiscard]] virtual std::optional<std::filesystem::path>
        resolvePath(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const = 0;
    [[nodiscard]] virtual bool exists(std::string_view plugin_id,
                                      const std::filesystem::path& relative_asset_path) const = 0;
    [[nodiscard]] virtual std::optional<std::string>
        readText(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const = 0;
    [[nodiscard]] virtual PluginAssetBytes readBytes(std::string_view plugin_id,
                                                     const std::filesystem::path& relative_asset_path) const = 0;
    [[nodiscard]] virtual TextureView texture(std::string_view plugin_id,
                                              const std::filesystem::path& relative_asset_path) = 0;
};

} // namespace luna::editor
