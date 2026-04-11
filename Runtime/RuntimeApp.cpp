#include "Runtime/RuntimeApp.h"

#include "Plugin/PluginBootstrap.h"
#include "Plugin/PluginRegistry.h"

namespace luna::runtime {

RuntimeApp::RuntimeApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna Runtime",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
      })
{}

void RuntimeApp::onInit()
{
    luna::PluginRegistry plugin_registry(m_service_registry);
    luna::registerResolvedPlugins(plugin_registry);

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

} // namespace luna::runtime

namespace luna {

Application* createApplication(int, char**)
{
    return new runtime::RuntimeApp();
}

} // namespace luna
