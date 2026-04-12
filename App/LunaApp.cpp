#include "App/LunaApp.h"

#include "Core/Log.h"
#include "Plugin/PluginBootstrap.h"
#include "Plugin/PluginRegistry.h"

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

void LunaApp::onInit()
{
    luna::PluginRegistry plugin_registry(m_service_registry, m_editor_registry.get());
    luna::registerResolvedPlugins(plugin_registry);

    if (plugin_registry.isImGuiRequested() && !enableImGui(plugin_registry.requestsMultiViewport())) {
        LUNA_CORE_ERROR("Failed to enable ImGui for the active plugin bundle");
        close();
        return;
    }

    for (const auto& layer : plugin_registry.layers()) {
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
