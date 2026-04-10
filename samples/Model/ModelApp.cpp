#include "samples/Model/ModelApp.h"
#include "samples/Model/ModelLayer.h"
#include "samples/Model/ModelRenderPass.h"

namespace luna::samples::model {

ModelApp::ModelApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna Model Sample",
          .m_window_width = 1'440,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_multi_viewport = false,
      })
{}

VulkanRenderer::InitializationOptions ModelApp::getRendererInitializationOptions()
{
    VulkanRenderer::InitializationOptions options{.m_render_graph_builder = buildModelRenderGraph};
    return options;
}

void ModelApp::onInit()
{
    pushLayer(std::make_unique<ModelLayer>());
}

} // namespace luna::samples::model

namespace luna {

Application* createApplication(int, char**)
{
    return new samples::model::ModelApp();
}

} // namespace luna
