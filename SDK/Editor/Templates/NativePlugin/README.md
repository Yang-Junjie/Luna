# Native Plugin Template

This template builds a `Runtime: Native` editor plugin that uses the Luna editor native C++ wrapper:

```cpp
#include "Luna/Editor/Native/NativePlugin.h"
```

The wrapper is header-only and sits on top of `EditorApi/EditorNativePluginApi.h`; the exported plugin symbol remains the C ABI entry point.

For a broader example that uses project, scene, selection, asset, plugin asset, viewport, and runtime viewport services, see `Plugins/Editor/NativeSample` in the Luna source tree.

Configure it against an installed SDK:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=<LunaEditorSDK install prefix>
cmake --build build --config Debug
```

Copy or keep the whole template directory as one editor plugin package. The package layout is:

```text
NativePlugin/
  editor-plugin.yaml
  Source/
  Binaries/
```

Change the plugin ID in both `editor-plugin.yaml` and `Source/NativeTemplatePlugin.cpp` before using this as a real plugin.
