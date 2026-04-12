#include "Plugin/PluginRegistry.h"

extern "C" void luna_register_luna_imgui(luna::PluginRegistry& registry)
{
    registry.requestImGui();
}
