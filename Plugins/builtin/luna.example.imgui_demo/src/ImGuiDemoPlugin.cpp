#include "ImGuiDemoPanel.h"

#include "Editor/EditorRegistry.h"
#include "Plugin/PluginRegistry.h"

extern "C" void luna_register_luna_example_imgui_demo(luna::PluginRegistry& registry)
{
    if (!registry.hasEditorRegistry()) {
        return;
    }

    registry.editor().addPanel<luna::example::ImGuiDemoPanel>(
        "luna.example.imgui_demo.panel",
        "ImGui Demo",
        true);
}
