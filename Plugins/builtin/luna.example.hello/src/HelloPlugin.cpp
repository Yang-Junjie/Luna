#include "Editor/EditorRegistry.h"
#include "HelloPanel.h"
#include "Plugin/PluginRegistry.h"

extern "C" void luna_register_luna_example_hello(luna::PluginRegistry& registry)
{
    if (!registry.hasEditorRegistry()) {
        return;
    }

    registry.editor().addPanel<luna::example::HelloPanel>("luna.example.hello.panel", "Hello", true);
}
