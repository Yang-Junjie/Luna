# Native Plugin Template

This template is the smallest recommended standalone `Runtime: Native` editor plugin package.
It uses the Luna editor native C++ wrapper:

```cpp
#include "Luna/Editor/Native/NativePlugin.h"
```

The wrapper is header-only and sits on top of `EditorApi/EditorNativePluginApi.h`; the exported plugin symbol remains the C ABI entry point.
Do not change the ABI version macros in plugin code.

This template demonstrates:

- a plugin manifest;
- a native dynamic library entry point;
- a registered command;
- a Window menu entry;
- a plugin-owned editor window;
- basic `EditorUi` controls through the native wrapper;
- plugin-owned assets through `host.pluginAssets()`.

For a broader example that uses project, scene, selection, asset, viewport, and runtime viewport services, see `Plugins/Editor/NativeSample` in the Luna source tree.

## Package Layout

Keep the whole directory as one editor plugin package:

```text
NativePlugin/
  editor-plugin.yaml
  CMakeLists.txt
  Source/
    NativeTemplatePlugin.cpp
  assets/
    welcome.txt
  Binaries/
    Win64/
    Linux/
    macOS/
```

`assets/` is optional, but it is the recommended place for files shipped with the plugin. Use `host.pluginAssets()` to read them. Do not read from the game project to find editor-plugin private files.

Before turning this into a real plugin, change the plugin ID in both:

- `editor-plugin.yaml`
- `Source/NativeTemplatePlugin.cpp`

Configure it against an installed SDK:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=<LunaEditorSDK install prefix>
cmake --build build --config Debug
```

The build writes the plugin binary into the package-local `Binaries/<platform>/` directory referenced by `editor-plugin.yaml`.

## Loading In LunaEditor

During development, point LunaEditor at the package directory or any parent directory containing editor plugin packages:

```powershell
LunaEditor.exe --editor-plugin-dir <path-to-NativePlugin>
```

or set `LUNA_EDITOR_PLUGIN_PATH`. Use `;` between paths on Windows and `:` on Linux/macOS.

For installed engine data, place editor plugins under:

```text
<EngineDataRoot>/Plugins/Editor/Installed/
```

Game projects should not store editor plugin packages.
