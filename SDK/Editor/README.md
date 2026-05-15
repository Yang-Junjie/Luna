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

The template in `Templates/NativePlugin` shows the expected package shape, manifest, binary output paths, and a minimal command/window/menu plugin using the C++ wrapper.

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
