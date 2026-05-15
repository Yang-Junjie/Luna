#include "Shell/EditorPluginManager.h"

#include "Shell/EditorBuiltinPluginRegistry.h"
#include "Shell/EditorPluginManifest.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace {

bool isSameOrNestedPath(const std::filesystem::path& path, const std::filesystem::path& root)
{
    const std::filesystem::path normalized_path = path.lexically_normal();
    const std::filesystem::path normalized_root = root.lexically_normal();

    auto path_it = normalized_path.begin();
    auto root_it = normalized_root.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }

    return true;
}

void attachBuiltinFactories(std::vector<luna::editor::EditorPluginPackage>& packages)
{
    for (luna::editor::EditorPluginPackage& package : packages) {
        if (luna::editor::EditorBuiltinPluginFactory factory =
                luna::editor::EditorBuiltinPluginRegistry::findFactory(package.id)) {
            package.create = std::move(factory);
        }
    }
}

void appendPackages(std::vector<luna::editor::EditorPluginPackage>& packages,
                    std::vector<luna::editor::EditorPluginPackage> discovered)
{
    packages.insert(packages.end(),
                    std::make_move_iterator(discovered.begin()),
                    std::make_move_iterator(discovered.end()));
}

} // namespace

namespace luna::editor {

std::vector<EditorPluginPackage> createEditorPluginPackages(const EditorEnginePaths& engine_paths)
{
    EditorPluginManifestLoader manifest_loader;
    std::vector<EditorPluginPackage> packages;

    for (const std::filesystem::path& root : engine_paths.official_editor_plugin_roots) {
        std::vector<EditorPluginPackage> official_packages = manifest_loader.loadPackagesFromRoot(root);
        attachBuiltinFactories(official_packages);
        appendPackages(packages, std::move(official_packages));
    }

    for (const std::filesystem::path& root : engine_paths.installed_editor_plugin_roots) {
        appendPackages(packages, manifest_loader.loadPackagesFromRoot(root));
    }

    for (const std::filesystem::path& root : engine_paths.development_editor_plugin_roots) {
        std::vector<EditorPluginPackage> development_packages = manifest_loader.loadPackagesFromRoot(root);
        for (const std::filesystem::path& official_root : engine_paths.official_editor_plugin_roots) {
            development_packages.erase(std::remove_if(development_packages.begin(),
                                                      development_packages.end(),
                                                      [&](const EditorPluginPackage& package) {
                                                          return isSameOrNestedPath(package.root_path, official_root);
                                                      }),
                                       development_packages.end());
        }
        attachBuiltinFactories(development_packages);
        appendPackages(packages, std::move(development_packages));
    }

    return packages;
}

std::vector<EditorPluginPackage> createEditorPluginPackages()
{
    return createEditorPluginPackages(resolveEditorEnginePaths(EditorStartupOptions{}));
}

} // namespace luna::editor
