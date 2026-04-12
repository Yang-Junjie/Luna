#include "Plugin/PluginBootstrap.h"
#include "Plugin/PluginRegistry.h"

extern "C" void luna_register_luna_runtime_core(luna::PluginRegistry& registry);

namespace luna {

void registerResolvedPlugins(PluginRegistry& registry)
{
    luna_register_luna_runtime_core(registry);
}

} // namespace luna
