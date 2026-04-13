#include "Plugin/PluginRegistry.h"
#include "RuntimeStaticMeshLayer.h"

#include <memory>

extern "C" void luna_register_luna_runtime_core(luna::PluginRegistry& registry)
{
    registry.addLayer("luna.runtime.static_mesh", [] {
        return std::make_unique<luna::runtime::RuntimeStaticMeshLayer>();
    });
}
