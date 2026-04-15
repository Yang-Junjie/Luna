#pragma once

#include "Core/Layer.h"
#include "Renderer/PluginRenderGraph.h"
#include "Renderer/VulkanRenderer.h"
#include "Plugin/ServiceRegistry.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace luna {

namespace editor {
class EditorRegistry;
}

class PluginRegistry {
public:
    using LayerFactory = std::function<std::unique_ptr<Layer>()>;
    using RenderGraphBuilderFactory = std::function<VulkanRenderer::RenderGraphBuilderCallback(bool include_imgui)>;

    struct LayerContribution {
        std::string m_id;
        bool m_overlay = false;
        LayerFactory m_factory;
    };

    struct RenderGraphProviderContribution {
        std::string m_id;
        RenderGraphBuilderFactory m_factory;
    };

    explicit PluginRegistry(ServiceRegistry& services, editor::EditorRegistry* editor_registry = nullptr);

    ServiceRegistry& services()
    {
        return m_services;
    }

    const ServiceRegistry& services() const
    {
        return m_services;
    }

    bool hasEditorRegistry() const
    {
        return m_editor_registry != nullptr;
    }

    void requestImGui(bool enable_multi_viewport = false)
    {
        m_imgui_requested = true;
        m_enable_multi_viewport = m_enable_multi_viewport || enable_multi_viewport;
    }

    bool isImGuiRequested() const
    {
        return m_imgui_requested;
    }

    bool requestsMultiViewport() const
    {
        return m_enable_multi_viewport;
    }

    editor::EditorRegistry& editor();
    const editor::EditorRegistry& editor() const;

    void addLayer(std::string id, LayerFactory factory);
    void addOverlay(std::string id, LayerFactory factory);
    void addRenderGraph(std::string id, render::RenderGraphFactory factory);
    void addRenderGraphProvider(std::string id, RenderGraphBuilderFactory factory);

    const std::vector<LayerContribution>& layers() const
    {
        return m_layers;
    }

    const std::vector<RenderGraphProviderContribution>& renderGraphProviders() const
    {
        return m_render_graph_providers;
    }

private:
    void addLayerContribution(std::string id, bool overlay, LayerFactory factory);
    void addRenderGraphProviderContribution(std::string id, RenderGraphBuilderFactory factory);

private:
    ServiceRegistry& m_services;
    editor::EditorRegistry* m_editor_registry = nullptr;
    bool m_imgui_requested = false;
    bool m_enable_multi_viewport = false;
    std::vector<LayerContribution> m_layers;
    std::vector<RenderGraphProviderContribution> m_render_graph_providers;
};

} // namespace luna
