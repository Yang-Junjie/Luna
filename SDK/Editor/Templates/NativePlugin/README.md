# Native Plugin Template

This template builds a `Runtime: Native` editor plugin that uses only the Luna editor C ABI header.

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
