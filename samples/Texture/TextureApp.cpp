#include "samples/Texture/TextureApp.h"

#include "samples/Common/SampleCameraLayer.h"
#include "samples/Texture/TextureRenderPass.h"

namespace luna::samples::texture {

TextureApp::TextureApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna Texture Sample",
          .m_window_width = 1'280,
          .m_window_height = 720,
          .m_maximized = false,
          .m_enable_multi_viewport = false,
          .m_render_graph_builder = buildTextureRenderGraph,
      })
{}

void TextureApp::onInit()
{
    pushLayer(std::make_unique<luna::samples::SampleCameraLayer>(luna::samples::SampleCameraLayerOptions{
        .m_title = "Texture",
        .m_description = "Loads assets/head.jpg and renders it on a textured quad.",
        .m_initial_position = glm::vec3{0.0f, 0.0f, 2.2f},
    }));
}

} // namespace luna::samples::texture

namespace luna {

Application* createApplication(int, char**)
{
    return new samples::texture::TextureApp();
}

} // namespace luna
