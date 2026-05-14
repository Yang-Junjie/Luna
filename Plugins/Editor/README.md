# Luna Editor Plugins

This directory is reserved for editor plugins that belong to the Luna editor environment.

Editor plugins placed here are not game project content. Runtime scripting backend plugins stay under `Plugins/Scripting`.

- `Official` contains the default Luna editor plugins. They are still `Runtime: BuiltinNative` source plugins compiled into `LunaEditor`, but they use the same package layout and public `Luna/EditorApi` surface as user plugins.
- `ExampleTool` is a source-level `BuiltinNative` editor plugin compiled into `LunaEditor`.
- `NativeSample` is a dynamic `Runtime: Native` editor plugin that validates the v1 C ABI, command/window registration, and `EditorUi` drawing.
