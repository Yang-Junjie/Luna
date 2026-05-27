#include "Shell/EditorPluginDependencyResolver.h"
#include "Shell/EditorPluginManager.h"

namespace luna::editor {

std::vector<std::string> missingEditorPluginDependencies(const EditorPluginPackage& package,
                                                         const std::unordered_set<std::string>& loaded_plugin_ids)
{
    std::vector<std::string> missing;
    for (const std::string& dependency : package.dependencies) {
        if (loaded_plugin_ids.find(dependency) == loaded_plugin_ids.end()) {
            missing.push_back(dependency);
        }
    }
    return missing;
}

bool areEditorPluginDependenciesLoaded(const EditorPluginPackage& package,
                                       const std::unordered_set<std::string>& loaded_plugin_ids)
{
    return missingEditorPluginDependencies(package, loaded_plugin_ids).empty();
}

} // namespace luna::editor
