# Luna Editor SDK

This directory is the starting point for the standalone editor plugin SDK.

The first supported standalone path is `Runtime: Native`.

Native dynamic plugins can use the header-only C++ wrapper:

```cpp
#include "Luna/Editor/Native/NativePlugin.h"
```

The wrapper stays on top of the C ABI and does not change ABI versioning or ownership rules. Plugins can still include the raw C ABI header directly when needed:

```cpp
#include "EditorApi/EditorNativePluginApi.h"
```

and build against the exported CMake target:

```cmake
find_package(LunaEditorSDK CONFIG REQUIRED)
target_link_libraries(MyEditorPlugin PRIVATE Luna::EditorSDK)
```

The template in `Templates/NativePlugin` shows the expected package shape, manifest, binary output paths, plugin-owned `assets/`, and a minimal command/window/menu/window-UI plugin using the C++ wrapper.

## Minimal Native Plugin Flow

1. Start from `Templates/NativePlugin`.
2. Change the plugin ID in `editor-plugin.yaml` and `Source/NativeTemplatePlugin.cpp`.
3. Build against an installed SDK:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=<LunaEditorSDK install prefix>
cmake --build build --config Debug
```

4. Run LunaEditor with the plugin package directory as a development root:

```powershell
LunaEditor.exe --editor-plugin-dir <path-to-NativePlugin>
```

You can also set `LUNA_EDITOR_PLUGIN_PATH`. It uses `;` on Windows and `:` on Linux/macOS.
Installed editor plugins belong under `<EngineDataRoot>/Plugins/Editor/Installed/`. Game projects should not store editor plugin packages.

The package directory is the unit of distribution:

```text
MyPlugin/
  editor-plugin.yaml
  CMakeLists.txt
  Source/
  assets/
  Binaries/
```

Files in `assets/` are private editor-plugin files and should be accessed through `host.pluginAssets()`.

Source-level `BuiltinNative` plugins are a separate integration path for plugins compiled with the editor source tree. They can register their factory with:

```cpp
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"
```

Those source-level plugins still need the editor source build because they use the C++ in-process plugin interface.

The SDK currently installs:

- `Luna::EditorSDK`
- `EditorApi/*.h`
- `Luna/Editor/*.h`
- `Luna/Editor/Native/*.h`
- `Templates/NativePlugin`

Current `Luna/Editor/Native` wrappers cover the v1 Native ABI basics used by real sample plugins:

- log, UI, commands, windows, menus
- project, project assets, plugin-owned assets
- scene, selection
- scene viewport and runtime viewport state

Two contract tests protect this boundary:

- `EditorSdkNativeTemplateContract` installs only the `LunaEditorSDK` component, configures the installed native template with `find_package(LunaEditorSDK CONFIG REQUIRED)`, and builds it as an external plugin.
- `EditorPluginBoundaryContract` scans editor plugin sources and SDK templates for editor-private includes and raw ImGui usage.

The goal is that native editor plugin authors can download the SDK package instead of the full engine source. Higher-level C++ wrappers and Lua editor plugin bindings should be layered on top of the same underlying Editor API instead of creating a separate plugin model.
