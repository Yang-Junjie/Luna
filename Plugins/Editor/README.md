# Luna Editor Plugins

This directory is reserved for editor plugins that belong to the Luna editor environment.

Editor plugins placed here are not game project content. Runtime scripting backend plugins stay under `Plugins/Scripting`.

- `Official` contains the default Luna editor plugins. They are still `Runtime: BuiltinNative` source plugins compiled into `LunaEditor`, but they use the same package layout and public `Luna/EditorApi` plus `SDK/Editor` surface as user plugins.
- `NativeSample` is the mainline dynamic `Runtime: Native` sample. It uses the SDK wrapper path to register commands, menus, windows, plugin assets, project assets, scene/selection tools, an independent scene viewport, and runtime viewport controls without editor-private headers or raw ImGui. See `NativeSample/README.md`.

Each plugin directory now owns its own `CMakeLists.txt` and package metadata, so official plugins and user source plugins follow the same physical shape.

Standalone native plugin authors should start from `SDK/Editor/Templates/NativePlugin` and consume an installed `LunaEditorSDK` package with `find_package(LunaEditorSDK CONFIG REQUIRED)`.
