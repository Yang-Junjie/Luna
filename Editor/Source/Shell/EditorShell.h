#pragma once

#include "EditorApi/EditorHost.h"

#include <initializer_list>
#include <memory>
#include <string_view>

namespace luna {

class LunaEditorLayer;

namespace editor {

class Plugin;
class RuntimeViewportService;
class Ui;
class WindowService;
class CommandService;
class HistoryService;
class MenuService;
class ScriptPluginService;
class RenderingService;
class SceneService;
class SelectionService;
class ViewportService;

class EditorShell final : public Host {
public:
    explicit EditorShell(LunaEditorLayer& editor_layer);
    ~EditorShell() override;

    Ui& ui() override;
    WindowService& windows() override;
    CommandService& commands() override;
    HistoryService& history() override;
    MenuService& menus() override;
    ScriptPluginService& scriptPlugins() override;
    RenderingService& rendering() override;
    SceneService& scene() override;
    SelectionService& selection() override;
    RuntimeViewportService& runtimeViewport() override;
    ViewportService& viewport() override;

    bool loadPlugin(std::unique_ptr<Plugin> plugin);
    void unloadPlugins();
    void update(float delta_seconds);
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
