#include "Editor/EditorRegistry.h"
#include "Editor/EditorShellLayer.h"
#include "Plugin/PluginRegistry.h"

#include <memory>

extern "C" void luna_register_luna_editor_shell(luna::PluginRegistry& registry)
{
    registry.requestImGui();

    if (!registry.hasEditorRegistry()) {
        return;
    }

    auto* editor_registry = &registry.editor();
    registry.addOverlay("luna.editor.shell", [editor_registry] {
        return std::make_unique<luna::editor::EditorShellLayer>(*editor_registry);
    });
}
