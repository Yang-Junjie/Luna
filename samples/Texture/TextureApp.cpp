#include "samples/Texture/TextureApp.h"
#include "samples/Texture/TextureLayer.h"
#include "samples/Texture/TextureRenderPass.h"

namespace luna::samples::texture {

TextureApp::TextureApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna Texture Sample",
          .m_window_width = 1'280,
          .m_window_height = 720,
          .m_maximized = false,
          .m_enable_multi_viewport = false,
      })
{}

VulkanRenderer::InitializationOptions TextureApp::getRendererInitializationOptions()
{
    VulkanRenderer::InitializationOptions options{.m_render_graph_builder = buildTextureRenderGraph};
    return options;
}

void TextureApp::onInit()
{
    pushLayer(std::make_unique<TextureLayer>());
}

} // namespace luna::samples::texture

namespace luna {

Application* createApplication(int, char**)
{
    return new samples::texture::TextureApp();
}

} // namespace luna
