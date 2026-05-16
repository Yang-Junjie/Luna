#pragma once

#include "EditorApi/EditorHost.h"
#include "EditorApi/EditorPlugin.h"
#include "EditorApi/EditorTypes.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace luna::editor {

class EditorPluginManagerHost : public Host {
public:
    ~EditorPluginManagerHost() override = default;

    virtual bool loadPlugin(std::unique_ptr<Plugin> plugin, const std::filesystem::path& root_path = {}) = 0;
    virtual void unloadPlugins() = 0;
    virtual void registerPluginAssetRoot(std::string_view plugin_id, const std::filesystem::path& root_path) = 0;
    virtual void cleanupPluginContributions(std::string_view owner_id) = 0;
    virtual ViewportId createSceneViewportForPlugin(std::string_view owner_id, std::string_view debug_name) = 0;
};

} // namespace luna::editor
