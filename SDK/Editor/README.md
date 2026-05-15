# Luna Editor SDK

This directory is the starting point for the standalone editor plugin SDK.

The first supported standalone path is `Runtime: Native`. Native dynamic plugins include the C ABI header:

```cpp
#include "EditorApi/EditorNativePluginApi.h"
```

and build against the exported CMake target:

```cmake
find_package(LunaEditorSDK CONFIG REQUIRED)
target_link_libraries(MyEditorPlugin PRIVATE Luna::EditorSDK)
```

The template in `Templates/NativePlugin` shows the expected package shape, manifest, binary output paths, and a minimal command/window/menu plugin.

Source-level `BuiltinNative` plugins are a separate integration path for plugins compiled with the editor source tree. They can register their factory with:

```cpp
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"
```

Those source-level plugins still need the editor source build because they use the C++ in-process plugin interface.

The SDK currently installs:

- `Luna::EditorSDK`
- `EditorApi/*.h`
- `Luna/Editor/*.h`
- `Templates/NativePlugin`

The goal is that native editor plugin authors can download the SDK package instead of the full engine source. Higher-level C++ wrappers and Lua editor plugin bindings should be layered on top of the same underlying Editor API instead of creating a separate plugin model.
