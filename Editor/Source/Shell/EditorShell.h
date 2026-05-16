#pragma once

#include "Asset/Asset.h"
#include "EditorApi/EditorPluginService.h"
#include "EditorApi/EditorTypes.h"
#include "Shell/EditorPluginManagerHost.h"

#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>
#include <string_view>

namespace luna {

class LunaEditorLayer;

namespace editor {

class AssetService;
class Plugin;
class RuntimeViewportService;
class Ui;
class WindowService;
class CommandService;
class HistoryService;
class MenuService;
class PluginAssetService;
class PluginService;
class ProjectService;
class ScriptPluginService;
class ScriptService;
class RenderingService;
class SceneService;
class SelectionService;
class SettingsService;
class ShortcutService;
class ViewportService;
class EditorSettingsStore;

class EditorShell final : public EditorPluginManagerHost {
public:
    EditorShell(LunaEditorLayer& editor_layer, EditorSettingsStore& settings_store);
    ~EditorShell() override;

    Ui& ui() override;
    AssetService& assets() override;
    WindowService& windows() override;
    CommandService& commands() override;
    HistoryService& history() override;
    MenuService& menus() override;
    PluginAssetService& pluginAssets() override;
    PluginService& plugins() override;
    ProjectService& project() override;
    ScriptPluginService& scriptPlugins() override;
    ScriptService& scripts() override;
    RenderingService& rendering() override;
    SceneService& scene() override;
    SelectionService& selection() override;
    SettingsService& settings() override;
    ShortcutService& shortcuts() override;
    RuntimeViewportService& runtimeViewport() override;
    ViewportService& viewport() override;

    bool loadPlugin(std::unique_ptr<Plugin> plugin, const std::filesystem::path& root_path = {}) override;
    void unloadPlugins() override;
    void registerPluginAssetRoot(std::string_view plugin_id, const std::filesystem::path& root_path) override;
    void cleanupPluginContributions(std::string_view owner_id) override;
    ViewportId createSceneViewportForPlugin(std::string_view owner_id, std::string_view debug_name) override;
    void setPluginInfoProvider(std::function<std::vector<PluginInfo>()> provider);
    void update(float delta_seconds);
    bool dispatchShortcuts();
    void drawMenuItems(std::string_view menu_path);
    void drawMenuBarItems(std::initializer_list<std::string_view> handled_roots);
    void drawWindowMenuItems();
    void drawWindows();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace editor
} // namespace luna
