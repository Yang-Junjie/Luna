#include "Plugin/PluginRegistry.h"
#include "Renderer/PluginRenderGraph.h"
#include "RuntimeStaticMeshLayer.h"

#include <memory>

extern "C" void luna_register_luna_runtime_core(luna::PluginRegistry& registry)
{
    registry.addRenderGraph("luna.runtime.core.graph", [](const luna::render::RenderGraphBuildInfo& build_info) {
        luna::render::RenderGraphDefinition graph;
        graph.addScenePass();
        if (build_info.m_include_imgui) {
            graph.addImGuiPass();
        }
        return graph;
    });

    registry.addLayer("luna.runtime.static_mesh", [] {
        return std::make_unique<luna::runtime::RuntimeStaticMeshLayer>();
    });
}
