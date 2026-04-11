#include "RuntimeClearColorLayer.h"

#include "Plugin/PluginRegistry.h"

#include <memory>

extern "C" void luna_register_luna_runtime_core(luna::PluginRegistry& registry)
{
    registry.addLayer("luna.runtime.clear_color", [] {
        return std::make_unique<luna::runtime::RuntimeClearColorLayer>();
    });
}
