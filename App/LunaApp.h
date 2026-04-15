#pragma once

#include "Core/Application.h"
#include "Editor/EditorRegistry.h"
#include "Plugin/PluginRegistry.h"
#include "Plugin/ServiceRegistry.h"

#include <memory>
#include <string>

namespace luna::app {

class LunaApp final : public Application {
public:
    LunaApp();

protected:
    bool onPreInitialize() override;
    VulkanRenderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;

private:
    bool ensurePluginBootstrap();
    bool resolveRenderGraphProvider();

private:
    luna::ServiceRegistry m_service_registry;
    std::shared_ptr<luna::editor::EditorRegistry> m_editor_registry;
    std::unique_ptr<luna::PluginRegistry> m_plugin_registry;
    VulkanRenderer::RenderGraphBuilderCallback m_active_render_graph_builder;
    std::string m_active_render_graph_provider_id;
    bool m_plugin_bootstrap_completed = false;
};

} // namespace luna::app
