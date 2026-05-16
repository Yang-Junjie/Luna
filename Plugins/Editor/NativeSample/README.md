# Native Sample Editor Plugin

`NativeSample` is the canonical dynamic native editor plugin sample for Luna.

It is intentionally built like an external SDK plugin:

- `Runtime: Native` in `editor-plugin.yaml`;
- package-local `assets/` and `Binaries/<platform>/` directories;
- `luna_add_editor_native_plugin()` from `SDK/Editor/cmake/LunaEditorSDKHelpers.cmake`;
- only public SDK headers, currently `Luna/Editor/Native/NativePlugin.h`;
- no editor-private headers and no raw `ImGui::` calls.

The sample is meant to stay same-level with user plugins. Official editor plugins may ship with Luna, but they should not rely on privileged editor internals when a public API exists.

## What It Demonstrates

- native ABI v1 entry point: `LunaCreateEditorPlugin`;
- scoped command, window, and menu registration;
- `host.ui()` drawing;
- plugin-owned assets through `host.pluginAssets()`;
- opened project information and project assets through `host.project()` and `host.assets()`;
- scene authoring and selection through `host.scene()` and `host.selection()`;
- editor viewport state plus an independent plugin-owned scene viewport through `host.viewport()`;
- runtime viewport state through `host.runtimeViewport()`;
- deterministic cleanup in `on_unload`.

Use this package when validating the real dynamic loading path inside the engine source tree. Use `SDK/Editor/Templates/NativePlugin` as the starting point for a new standalone plugin outside the engine source tree.

The full authoring guide is in `SDK/Editor/docs/native-plugin-authoring.md`.
