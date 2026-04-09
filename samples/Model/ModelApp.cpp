#include "samples/Model/ModelApp.h"

#include "samples/Common/SampleCameraLayer.h"
#include "samples/Model/ModelRenderPass.h"

namespace luna::samples::model {

ModelApp::ModelApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna Model Sample",
          .m_window_width = 1'440,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_multi_viewport = false,
          .m_render_graph_builder = buildModelRenderGraph,
      })
{}

void ModelApp::onInit()
{
    pushLayer(std::make_unique<luna::samples::SampleCameraLayer>(luna::samples::SampleCameraLayerOptions{
        .m_title = "Model",
        .m_description = "Loads assets/basicmesh.glb, uploads its materials, and renders the mesh with depth testing.",
        .m_initial_position = glm::vec3{0.0f, 0.5f, 4.5f},
    }));
}

} // namespace luna::samples::model

namespace luna {

Application* createApplication(int, char**)
{
    return new samples::model::ModelApp();
}

} // namespace luna
