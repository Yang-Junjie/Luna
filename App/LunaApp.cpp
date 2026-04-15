#include "App/LunaApp.h"
#include "Core/Log.h"
#include "Plugin/PluginBootstrap.h"
#include "Plugin/PluginRegistry.h"

#include <algorithm>
#include <utility>

namespace {

const luna::PluginRegistry::RenderGraphProviderContribution* findRenderGraphProvider(
    const std::vector<luna::PluginRegistry::RenderGraphProviderContribution>& providers, const char* provider_id)
{
    if (provider_id == nullptr || provider_id[0] == '\0') {
        return nullptr;
    }

    const auto it = std::find_if(
        providers.begin(), providers.end(), [provider_id](const luna::PluginRegistry::RenderGraphProviderContribution& provider) {
            return provider.m_id == provider_id;
        });
    return it != providers.end() ? &(*it) : nullptr;
}

} // namespace

namespace luna::app {

LunaApp::LunaApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = false,
      }),
      m_editor_registry(std::make_shared<luna::editor::EditorRegistry>())
{
    m_service_registry.add<luna::editor::EditorRegistry>(m_editor_registry);
}

bool LunaApp::onPreInitialize()
{
    if (!ensurePluginBootstrap()) {
        return false;
    }

    if (m_plugin_registry != nullptr && m_plugin_registry->isImGuiRequested()) {
        enableImGui(m_plugin_registry->requestsMultiViewport());
    }

    return true;
}

VulkanRenderer::InitializationOptions LunaApp::getRendererInitializationOptions()
{
    if (!ensurePluginBootstrap()) {
        return {};
    }

    return VulkanRenderer::InitializationOptions{
        .m_render_graph_builder = m_active_render_graph_builder,
    };
}

bool LunaApp::ensurePluginBootstrap()
{
    if (m_plugin_bootstrap_completed) {
        return true;
    }

    m_plugin_registry = std::make_unique<luna::PluginRegistry>(m_service_registry, m_editor_registry.get());
    luna::registerResolvedPlugins(*m_plugin_registry);

    if (!resolveRenderGraphProvider()) {
        m_plugin_registry.reset();
        return false;
    }

    m_plugin_bootstrap_completed = true;
    return true;
}

bool LunaApp::resolveRenderGraphProvider()
{
    m_active_render_graph_builder = {};
    m_active_render_graph_provider_id.clear();

    if (m_plugin_registry == nullptr) {
        LUNA_CORE_ERROR("Cannot resolve a render-graph provider before plugin bootstrap");
        return false;
    }

    const auto& providers = m_plugin_registry->renderGraphProviders();
    const char* preferred_provider_id = luna::getResolvedRenderGraphProviderId();

    if (providers.empty()) {
        if (preferred_provider_id != nullptr && preferred_provider_id[0] != '\0') {
            LUNA_CORE_ERROR("Bundle requested render-graph provider '{}' but no providers were registered", preferred_provider_id);
            return false;
        }

        LUNA_CORE_INFO("No render-graph provider was selected; falling back to the internal default render graph");
        return true;
    }

    const luna::PluginRegistry::RenderGraphProviderContribution* selected_provider = nullptr;
    if (preferred_provider_id != nullptr && preferred_provider_id[0] != '\0') {
        selected_provider = findRenderGraphProvider(providers, preferred_provider_id);
        if (selected_provider == nullptr) {
            LUNA_CORE_ERROR("Bundle requested unknown render-graph provider '{}'", preferred_provider_id);
            return false;
        }
    } else if (providers.size() == 1) {
        selected_provider = &providers.front();
    } else {
        LUNA_CORE_ERROR("Multiple render-graph providers were registered but the bundle did not select one");
        for (const auto& provider : providers) {
            LUNA_CORE_ERROR("  candidate provider: {}", provider.m_id);
        }
        return false;
    }

    if (selected_provider == nullptr) {
        LUNA_CORE_ERROR("Failed to determine the active render-graph provider");
        return false;
    }

    auto builder = selected_provider->m_factory(m_plugin_registry->isImGuiRequested());
    if (!builder) {
        LUNA_CORE_ERROR("Render-graph provider '{}' returned an empty builder callback", selected_provider->m_id);
        return false;
    }

    m_active_render_graph_provider_id = selected_provider->m_id;
    m_active_render_graph_builder = std::move(builder);
    LUNA_CORE_INFO("Selected render-graph provider '{}'", m_active_render_graph_provider_id);
    return true;
}

void LunaApp::onInit()
{
    if (!ensurePluginBootstrap()) {
        LUNA_CORE_ERROR("Plugin bootstrap was not completed before application startup");
        close();
        return;
    }

    for (const auto& layer : m_plugin_registry->layers()) {
        if (!layer.m_factory) {
            continue;
        }

        auto instance = layer.m_factory();
        if (instance == nullptr) {
            continue;
        }

        if (layer.m_overlay) {
            pushOverlay(std::move(instance));
        } else {
            pushLayer(std::move(instance));
        }
    }
}

} // namespace luna::app

namespace luna {

Application* createApplication(int, char**)
{
    return new app::LunaApp();
}

} // namespace luna
