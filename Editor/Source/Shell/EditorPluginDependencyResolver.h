#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace luna::editor {

struct EditorPluginPackage;

[[nodiscard]] std::vector<std::string> missingEditorPluginDependencies(
    const EditorPluginPackage& package,
    const std::unordered_set<std::string>& loaded_plugin_ids);

[[nodiscard]] bool areEditorPluginDependenciesLoaded(
    const EditorPluginPackage& package,
    const std::unordered_set<std::string>& loaded_plugin_ids);

} // namespace luna::editor
