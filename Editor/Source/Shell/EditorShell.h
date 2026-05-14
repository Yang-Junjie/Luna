#pragma once

#include "Asset/Asset.h"
#include "EditorApi/EditorHost.h"
#include "EditorApi/EditorTypes.h"

#include <filesystem>
#include <initializer_list>
#include <memory>
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
class ProjectService;
class ScriptPluginService;
class ScriptService;
class RenderingService;
class SceneService;
class SelectionService;
class ShortcutService;
class ViewportService;

class EditorShell final : public Host {
public:
    explicit EditorShell(LunaEditorLayer& editor_layer);
    ~EditorShell() override;

    Ui& ui() override;
    AssetService& assets() override;
    WindowService& windows() override;
    CommandService& commands() override;
    HistoryService& history() override;
    MenuService& menus() override;
    PluginAssetService& pluginAssets() override;
    ProjectService& project() override;
    ScriptPluginService& scriptPlugins() override;
    ScriptService& scripts() override;
    RenderingService& rendering() override;
    SceneService& scene() override;
    SelectionService& selection() override;
    ShortcutService& shortcuts() override;
    RuntimeViewportService& runtimeViewport() override;
    ViewportService& viewport() override;

    bool loadPlugin(std::unique_ptr<Plugin> plugin, const std::filesystem::path& root_path = {});
    void unloadPlugins();
    void registerPluginAssetRoot(std::string_view plugin_id, const std::filesystem::path& root_path);
    void cleanupPluginContributions(std::string_view owner_id);
    ViewportId createSceneViewportForPlugin(std::string_view owner_id, std::string_view debug_name);
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
