#include "Core/Log.h"
#include "Shell/EditorBuiltinPluginRegistry.h"

#include <unordered_map>
#include <utility>

namespace {

std::unordered_map<std::string, luna::editor::EditorBuiltinPluginFactory>& registeredFactories()
{
    static std::unordered_map<std::string, luna::editor::EditorBuiltinPluginFactory> factories;
    return factories;
}

} // namespace

namespace luna::editor {

bool registerBuiltinEditorPluginFactory(std::string plugin_id, EditorBuiltinPluginFactory factory)
{
    if (plugin_id.empty() || !factory) {
        return false;
    }

    auto& factories = registeredFactories();
    const auto [it, inserted] = factories.emplace(std::move(plugin_id), std::move(factory));
    if (!inserted) {
        LUNA_EDITOR_WARN("Ignoring duplicate built-in editor plugin factory '{}'", it->first);
        return false;
    }

    return true;
}

EditorBuiltinPluginFactory EditorBuiltinPluginRegistry::findFactory(std::string_view plugin_id)
{
    auto& factories = registeredFactories();
    const auto it = factories.find(std::string(plugin_id));
    return it != factories.end() ? it->second : EditorBuiltinPluginFactory{};
}

EditorBuiltinPluginFactoryRegistration::EditorBuiltinPluginFactoryRegistration(std::string plugin_id,
                                                                               EditorBuiltinPluginFactory factory)
{
    (void) registerBuiltinEditorPluginFactory(std::move(plugin_id), std::move(factory));
}

} // namespace luna::editor
