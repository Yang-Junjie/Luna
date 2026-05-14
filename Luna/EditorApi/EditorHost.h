#pragma once

namespace luna::editor {

class AssetService;
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
class RuntimeViewportService;
class Ui;
class ViewportService;
class WindowService;

class Host {
public:
    virtual ~Host() = default;

    virtual Ui& ui() = 0;
    virtual AssetService& assets() = 0;
    virtual WindowService& windows() = 0;
    virtual CommandService& commands() = 0;
    virtual HistoryService& history() = 0;
    virtual MenuService& menus() = 0;
    virtual PluginAssetService& pluginAssets() = 0;
    virtual ProjectService& project() = 0;
    virtual ScriptPluginService& scriptPlugins() = 0;
    virtual ScriptService& scripts() = 0;
    virtual RenderingService& rendering() = 0;
    virtual SceneService& scene() = 0;
    virtual SelectionService& selection() = 0;
    virtual ShortcutService& shortcuts() = 0;
    virtual RuntimeViewportService& runtimeViewport() = 0;
    virtual ViewportService& viewport() = 0;
};

} // namespace luna::editor
