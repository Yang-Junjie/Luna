#include "samples/Triangle/TriangleApp.h"

#include "samples/Triangle/TriangleLayer.h"
#include "samples/Triangle/TriangleRenderPass.h"

namespace luna::samples::triangle {

TriangleApp::TriangleApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna Triangle Sample",
          .m_window_width = 1'280,
          .m_window_height = 720,
          .m_maximized = false,
          .m_enable_multi_viewport = false,
          .m_render_graph_builder = buildTriangleRenderGraph,
      })
{}

void TriangleApp::onInit()
{
    pushLayer(std::make_unique<TriangleLayer>());
}

} // namespace luna::samples::triangle

namespace luna {

Application* createApplication(int, char**)
{
    return new samples::triangle::TriangleApp();
}

} // namespace luna
