#pragma once

#include "Shell/EditorPluginManager.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace luna::editor {

class EditorPluginManifestLoader final {
public:
    [[nodiscard]] std::optional<EditorPluginPackage> loadPackage(const std::filesystem::path& manifest_path) const;
    [[nodiscard]] std::vector<EditorPluginPackage> loadPackagesFromRoot(const std::filesystem::path& root_path) const;
};

} // namespace luna::editor
