#include "Plugin/PluginBootstrap.h"
#include "Plugin/PluginRegistry.h"

extern "C" void luna_register_luna_editor_core(luna::PluginRegistry& registry);
extern "C" void luna_register_luna_example_hello(luna::PluginRegistry& registry);
extern "C" void luna_register_luna_example_imgui_demo(luna::PluginRegistry& registry);

namespace luna {

void registerResolvedPlugins(PluginRegistry& registry)
{
    luna_register_luna_editor_core(registry);
    luna_register_luna_example_hello(registry);
    luna_register_luna_example_imgui_demo(registry);
}

} // namespace luna
