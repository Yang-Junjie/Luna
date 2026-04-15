#include "Core/Log.h"
#include "Plugin/PluginRegistry.h"
#include "Renderer/PluginRenderGraphInternal.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace luna {

PluginRegistry::PluginRegistry(ServiceRegistry& services, editor::EditorRegistry* editor_registry)
    : m_services(services),
      m_editor_registry(editor_registry)
{}

editor::EditorRegistry& PluginRegistry::editor()
{
    if (m_editor_registry == nullptr) {
        throw std::runtime_error("Editor registry is not available for this host");
    }

    return *m_editor_registry;
}

const editor::EditorRegistry& PluginRegistry::editor() const
{
    if (m_editor_registry == nullptr) {
        throw std::runtime_error("Editor registry is not available for this host");
    }

    return *m_editor_registry;
}

void PluginRegistry::addLayer(std::string id, LayerFactory factory)
{
    addLayerContribution(std::move(id), false, std::move(factory));
}

void PluginRegistry::addOverlay(std::string id, LayerFactory factory)
{
    addLayerContribution(std::move(id), true, std::move(factory));
}

void PluginRegistry::addRenderGraphProvider(std::string id, RenderGraphBuilderFactory factory)
{
    addRenderGraphProviderContribution(std::move(id), std::move(factory));
}

void PluginRegistry::addRenderGraph(std::string id, render::RenderGraphFactory factory)
{
    addRenderGraphProviderContribution(
        std::move(id),
        [factory = std::move(factory)](bool include_imgui) {
            return render::detail::createRenderGraphBuilderCallback(factory, include_imgui);
        });
}

void PluginRegistry::addLayerContribution(std::string id, bool overlay, LayerFactory factory)
{
    if (!factory) {
        LUNA_CORE_WARN("Ignoring plugin layer contribution '{}' because it has no factory", id);
        return;
    }

    const auto duplicate = std::find_if(m_layers.begin(), m_layers.end(), [&id](const LayerContribution& layer) {
        return layer.m_id == id;
    });

    if (duplicate != m_layers.end()) {
        LUNA_CORE_WARN("Ignoring duplicate plugin layer contribution '{}'", id);
        return;
    }

    m_layers.push_back(LayerContribution{.m_id = std::move(id), .m_overlay = overlay, .m_factory = std::move(factory)});
}

void PluginRegistry::addRenderGraphProviderContribution(std::string id, RenderGraphBuilderFactory factory)
{
    if (!factory) {
        LUNA_CORE_WARN("Ignoring render-graph provider '{}' because it has no factory", id);
        return;
    }

    const auto duplicate =
        std::find_if(m_render_graph_providers.begin(), m_render_graph_providers.end(), [&id](const RenderGraphProviderContribution& provider) {
            return provider.m_id == id;
        });

    if (duplicate != m_render_graph_providers.end()) {
        LUNA_CORE_WARN("Ignoring duplicate render-graph provider '{}'", id);
        return;
    }

    m_render_graph_providers.push_back(
        RenderGraphProviderContribution{.m_id = std::move(id), .m_factory = std::move(factory)});
}

} // namespace luna
