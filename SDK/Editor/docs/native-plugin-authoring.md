# Native Editor Plugin Authoring

This guide describes the supported standalone authoring path for Luna editor plugins using the native SDK.

The goal is a high-freedom editor plugin model:

- official editor plugins and user editor plugins use the same API surface;
- plugins can create editor windows, commands, menus, tools, and custom viewports;
- plugins talk to the editor through `LunaEditorHostApi` and the header-only C++ wrapper;
- plugins do not include editor-private headers or raw ImGui.

Editor plugins are only for LunaEditor. They are separate from runtime script plugins and should not be stored in game projects.

## Package Layout

An editor plugin package is a directory containing one manifest:

```text
MyEditorPlugin/
  editor-plugin.yaml
  CMakeLists.txt
  Source/
    MyEditorPlugin.cpp
  assets/
  Binaries/
    Win64/
    Linux/
    macOS/
```

`assets/` is optional. Use it for files shipped with the editor plugin itself: presets, icons, small text files, preview data, default templates, and other private editor tooling resources.

Project assets are different. Use `host.assets()` for files that belong to the opened game project, and `host.pluginAssets()` for files that belong to the plugin package.

Installed editor plugins should live under:

```text
<EngineDataRoot>/Plugins/Editor/Installed/
```

During development, pass a package directory or a parent directory containing packages:

```powershell
LunaEditor.exe --editor-plugin-dir <path-to-plugin-package>
LunaEditor.exe --editor-plugin-dir <path-containing-editor-plugin-packages>
```

You can pass `--editor-plugin-dir` more than once. You can also set `LUNA_EDITOR_PLUGIN_PATH`; it accepts the same package-or-parent roots and uses `;` on Windows and `:` on Linux/macOS.
Installed packages use the same manifest format and are discovered from `<EngineDataRoot>/Plugins/Editor/Installed/<PluginFolder>/editor-plugin.yaml`.

## Manifest

`editor-plugin.yaml` declares the package identity, runtime, and platform entry binaries:

```yaml
EditorPlugin:
  Id: com.example.my-tool
  DisplayName: My Tool
  Runtime: Native
  Version: 0.1.0
  Enabled: true
  Entry:
    Windows:
      x64: Binaries/Win64/MyTool.dll
      arm64: Binaries/Win64/MyTool.dll
    Linux:
      x64: Binaries/Linux/libMyTool.so
      arm64: Binaries/Linux/libMyTool.so
    macOS:
      x64: Binaries/macOS/libMyTool.dylib
      arm64: Binaries/macOS/libMyTool.dylib
```

Keep the manifest `Id` stable. Use a reverse-DNS style ID and use the same prefix for command IDs, window IDs, viewport IDs, and menu-owned identifiers.

`Runtime: Native` means the entry binary exports `LunaCreateEditorPlugin`.

## CMake

The recommended build uses the installed SDK helper:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyTool LANGUAGES CXX)

find_package(LunaEditorSDK CONFIG REQUIRED)

luna_add_editor_native_plugin(
    TARGET MyTool
    OUTPUT_NAME MyTool
    SOURCES
        Source/MyTool.cpp
)
```

`luna_add_editor_native_plugin` creates a shared library, links `Luna::EditorSDK`, enables C++20, and writes the binary to the package-local `Binaries/<platform>/` path expected by the manifest.

Configure an external plugin against an installed SDK:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=<LunaEditorSDK install prefix>
cmake --build build --config Debug
```

Advanced builds can link `Luna::EditorSDK` manually, but the helper is the stable path used by the SDK template and contract tests.

## Entry Point

Native plugins include the wrapper:

```cpp
#include "Luna/Editor/Native/NativePlugin.h"
```

The exported entry point fills a plugin descriptor:

```cpp
namespace native = luna::editor::native;

namespace {

constexpr const char* kPluginId = "com.example.my-tool";
constexpr const char* kDisplayName = "My Tool";
constexpr const char* kVersion = "0.1.0";

struct PluginState {
    native::RegisteredCommand command;
    native::RegisteredWindow window;
    native::RegisteredMenuItemsForCommand menu_items;

    void cleanup() noexcept
    {
        menu_items.reset();
        window.reset();
        command.reset();
    }
};

PluginState g_state{};

void executeOpenWindow(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    host.windows().setOpen("com.example.my-tool.window", true);
}

int onLoad(void* user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<PluginState*>(user_data);
    const native::Host host(host_api);
    if (state == nullptr || !host.valid()) {
        return 0;
    }

    state->cleanup();
    host.log().info("My Tool loaded.");
    return 1;
}

void onUnload(void* user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<PluginState*>(user_data);
    const native::Host host(host_api);
    if (state != nullptr) {
        state->cleanup();
    }
    host.log().info("My Tool unloaded.");
}

} // namespace

extern "C" LUNA_EDITOR_PLUGIN_EXPORT int LunaCreateEditorPlugin(uint32_t host_api_version,
                                                                const LunaEditorHostApi* host_api,
                                                                LunaEditorPluginApi* out_plugin_api)
{
    if (!native::isCompatibleHost(host_api_version, host_api) || out_plugin_api == nullptr) {
        return 0;
    }

    native::PluginDescriptor plugin{};
    plugin.plugin_id = kPluginId;
    plugin.display_name = kDisplayName;
    plugin.version = kVersion;
    plugin.user_data = &g_state;
    plugin.on_load = &onLoad;
    plugin.on_unload = &onUnload;

    return native::fillPluginApi(plugin, out_plugin_api) ? 1 : 0;
}
```

`on_load` is the right place to register editor contributions. `on_unload` must release them.

## Scoped Registrations

Prefer scoped registration wrappers. They make partial `on_load` failure and unload cleanup deterministic:

```cpp
native::CommandDescriptor command{};
command.id = "com.example.my-tool.open";
command.label = "Open My Tool";
command.description = "Opens the My Tool window.";
command.user_data = state;
command.execute = &executeOpenWindow;

native::RegisteredCommand command_registration = host.commands().registerScoped(command);
if (!command_registration) {
    host.log().error("Failed to register command.");
    return 0;
}

native::WindowDescriptor window{};
window.id = "com.example.my-tool.window";
window.title = "My Tool";
window.default_open = true;
window.default_size = native::vec2(360.0f, 240.0f);
window.user_data = state;
window.draw = &drawWindow;

native::RegisteredWindow window_registration = host.windows().registerScoped(window);
if (!window_registration) {
    host.log().error("Failed to register window.");
    return 0;
}

native::MenuItemDescriptor menu_item{};
menu_item.menu_path = "Tools/My Tool";
menu_item.command_id = "com.example.my-tool.open";
menu_item.label = "Open My Tool";

native::RegisteredMenuItemsForCommand menu_registration =
    host.menus().addScopedItemsForCommand(menu_item);
if (!menu_registration) {
    host.log().error("Failed to register menu item.");
    return 0;
}

state->command = std::move(command_registration);
state->window = std::move(window_registration);
state->menu_items = std::move(menu_registration);
```

Store handles in plugin state. Reset them in the order that matches your ownership. For example, if a window owns an independent viewport, reset the viewport first, then unregister menu items, windows, and commands.

## Editor UI

Plugins draw UI through `host.ui()`:

```cpp
void drawWindow(void* user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<PluginState*>(user_data);
    const native::Host host(host_api);
    const native::Ui ui = host.ui();
    if (state == nullptr || !ui.canDrawText()) {
        return;
    }

    ui.text("My Tool");
    ui.separator();

    if (ui.button("Run", native::fillWidth(), LunaEditorButtonVariant_Primary)) {
        host.log().info("Run clicked.");
    }
}
```

Do not include `imgui.h` and do not call `ImGui::` directly from SDK plugins or official plugin packages. The editor shell owns the UI backend, input routing, styling, drag/drop integration, and cross-platform details. `host.ui()` is the plugin-facing UI API.

The current wrapper covers common controls such as text, disabled text, wrapped text, separators, buttons, checkbox, input text, sliders, drag floats, combo/selectable, tree nodes, tooltips, popups, menus, drag/drop source and target helpers, image drawing, tables, sections, available content size, same-line layout, and width helpers.

## Assets

Use `host.pluginAssets()` for files inside the plugin package:

```cpp
if (host.pluginAssets().available()) {
    const std::string text = host.pluginAssets().readText("welcome.txt");
    if (!text.empty()) {
        host.ui().textWrapped(text.c_str());
    }
}
```

Use `host.assets()` for opened project assets:

```cpp
if (host.assets().available()) {
    const std::vector<native::AssetInfo> assets =
        host.assets().list(LunaEditorAssetType_None, false);

    for (const native::AssetInfo& asset : assets) {
        host.ui().text(asset.label.c_str());
    }
}
```

The project asset service is intentionally general. It is not a Content Browser-only API. Use it to enumerate assets, inspect metadata, resolve project asset paths, refresh asset state, query asset handles, and start editor asset drag/drop payloads.

## Scene And Selection

Scene and selection APIs let plugins build editor tools without depending on editor panels:

```cpp
const uint64_t entity = host.scene().createEntity("Generated Entity");
if (entity != 0u) {
    host.selection().selectEntity(entity);
}

if (host.scene().entityExists(entity)) {
    const native::SceneEntityInfo info = host.scene().entityInfo(entity);
    host.ui().text(info.name.c_str());
}
```

Use scene APIs for editor-facing scene operations: enumerate entities, read entity details, create/delete/reparent/rename entities, access transforms, and inspect supported camera, light, and mesh component state.

Keep runtime scripting plugins separate from editor plugins. Editor plugins may inspect and author scene data through the editor API, but they should not become a second runtime scripting system.

## Viewports

A plugin can create an independent scene viewport:

```cpp
struct PluginState {
    native::SceneViewportHandle preview_viewport;
};

if (!state->preview_viewport) {
    state->preview_viewport = host.viewport().createScopedSceneViewport("MyToolPreview");
}

const native::Vec2 available = host.ui().contentRegionAvail();
const float width = available.x > 64.0f ? available.x : 320.0f;
const float height = width * 0.5625f;

LunaEditorViewportPresentation presentation = native::makeViewportPresentation();
if (state->preview_viewport &&
    host.viewport().syncSceneViewport(state->preview_viewport.id(),
                                      static_cast<uint32_t>(width),
                                      static_cast<uint32_t>(height),
                                      &presentation) &&
    presentation.presentable != 0) {
    host.ui().image(presentation.scene_texture, native::vec2(width, height));
}
```

A viewport is a presentation surface plus editor-owned backing state. It does not have to mean "the main editor viewport". This keeps future tools open-ended: preview windows, material tools, scene-specific inspectors, image-only panels, and other editor surfaces can use the viewport API without stealing state from the main viewport.

Do not cache raw render resources outside the lifetime described by the viewport API. Sync the viewport each frame you draw it, then present the returned texture through `host.ui().image()`.

## Runtime Viewport

Runtime viewport state is exposed separately:

```cpp
const bool requested = host.runtimeViewport().requested();
const size_t entity_count = host.runtimeViewport().entityCount();
```

Use this for editor tools that need to observe or coordinate with runtime viewport state. Do not assume the runtime viewport and scene viewport are the same object.

## ABI V1 Rules

Do not bump native ABI version macros in plugin code.

Follow these compatibility rules:

- export only `LunaCreateEditorPlugin`;
- treat `LunaEditorHostApi` as the boundary;
- check host compatibility with `native::isCompatibleHost`;
- use `native::fillPluginApi` to fill `LunaEditorPluginApi`;
- do not transfer STL ownership across the binary boundary;
- do not free memory owned by the host;
- do not cache short-lived host pointers or returned C strings beyond the documented call scope;
- prefer wrapper return types that copy data into plugin-owned C++ objects;
- use struct size and version checks when working directly with the raw C ABI;
- release all registered contributions in `on_unload`.

The C++ wrapper is header-only and stays on top of ABI v1. Higher-level native helpers and future Lua editor plugin bindings should layer over the same editor API instead of creating a separate plugin model.

## Cross-Platform Notes

Do not hard-code Windows-only paths. Use manifest `Entry` paths per platform and keep package-relative paths portable.

Use `/` in manifest paths. Keep plugin asset paths relative to `assets/`.

Expected binary names:

- Windows: `Binaries/Win64/MyTool.dll`
- Linux: `Binaries/Linux/libMyTool.so`
- macOS: `Binaries/macOS/libMyTool.dylib`

Avoid platform APIs in plugin code unless the plugin feature really needs them. The editor shell should own platform-specific details such as windows, input, UI backend, drag/drop routing, and graphics integration.

## Debugging

Use the Plugin Manager diagnostics window:

```text
Window -> Plugin Manager
```

or the registered plugin manager command if your editor layout exposes it through another menu.

Useful diagnostics:

- manifest discovery and load state;
- native dynamic library path;
- missing entry symbol;
- failed `on_load`;
- registered window/command/menu behavior;
- editor log messages from `host.log()`.

The editor log is written under the editor build output logs directory, for example:

```text
build/Editor/logs/luna.log
```

For development load issues, verify:

- `--editor-plugin-dir` points at the package or a parent directory containing packages;
- `LUNA_EDITOR_PLUGIN_PATH` separators match the platform;
- `editor-plugin.yaml` `Entry` paths match the actual binary output;
- the plugin exports `LunaCreateEditorPlugin`;
- the plugin ID in the manifest matches the code-level descriptor;
- `on_load` returns `1`.

## Reference Template

Start from:

```text
SDK/Editor/Templates/NativePlugin
```

The template demonstrates:

- manifest shape;
- SDK CMake helper usage;
- native entry point;
- command/window/menu registration;
- scoped cleanup;
- `host.ui()` drawing;
- plugin assets;
- project assets;
- scene and selection operations;
- independent scene viewport preview;
- runtime viewport state reads.

Contract tests build and load this template against the installed SDK, so it is the best reference for the currently supported standalone native plugin path.
