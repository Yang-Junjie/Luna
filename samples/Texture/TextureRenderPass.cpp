#include "samples/Texture/TextureRenderPass.h"

#include "Core/Application.h"
#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/ImageLoader.h"
#include "Renderer/RenderGraphBuilder.h"
#include "samples/Common/SampleCommon.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {

struct TextureVertex {
    glm::vec3 m_position;
    glm::vec2 m_uv;
};

class TextureRenderPass final : public VulkanAbstractionLayer::RenderPass {
public:
    explicit TextureRenderPass(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
        : m_surface_format(build_info.m_surface_format),
          m_width(build_info.m_framebuffer_width),
          m_height(build_info.m_framebuffer_height),
          m_shader(luna::samples::loadGraphicsShader(
              luna::samples::projectRoot() / "samples" / "Texture" / "Shaders" / "TexturedQuad.vert",
              luna::samples::projectRoot() / "samples" / "Texture" / "Shaders" / "TexturedQuad.frag")),
          m_vertex_buffer(
              sizeof(k_vertices),
              VulkanAbstractionLayer::BufferUsage::VERTEX_BUFFER,
              VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
          m_index_buffer(
              sizeof(k_indices),
              VulkanAbstractionLayer::BufferUsage::INDEX_BUFFER,
              VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
          m_texture(luna::samples::createTexture(
              VulkanAbstractionLayer::ImageLoader::LoadImageFromFile(luna::samples::assetPath("head.jpg").string())))
    {
        m_vertex_buffer.CopyData(reinterpret_cast<const uint8_t*>(k_vertices), sizeof(k_vertices), 0);
        m_index_buffer.CopyData(reinterpret_cast<const uint8_t*>(k_indices), sizeof(k_indices), 0);
    }

    void SetupPipeline(VulkanAbstractionLayer::PipelineState pipeline) override
    {
        pipeline.Shader = m_shader;
        pipeline.VertexBindings = {{VulkanAbstractionLayer::VertexBinding::Rate::PER_VERTEX, VulkanAbstractionLayer::VertexBinding::BindingRangeAll}};
        pipeline.DescriptorBindings.Bind(
            0,
            "albedo_texture",
            m_texture.m_sampler,
            VulkanAbstractionLayer::UniformType::COMBINED_IMAGE_SAMPLER);
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", VulkanAbstractionLayer::ClearColor{0.05f, 0.07f, 0.10f, 1.0f});
    }

    void ResolveResources(VulkanAbstractionLayer::ResolveState resolve) override
    {
        resolve.Resolve("albedo_texture", m_texture.m_image);
    }

    void BeforeRender(VulkanAbstractionLayer::RenderPassState state) override
    {
        m_texture.uploadIfNeeded(state.Commands);
    }

    void OnRender(VulkanAbstractionLayer::RenderPassState state) override
    {
        const auto& scene_color = state.GetAttachment("scene_color");
        const auto& camera = luna::Application::get().getRenderer().getMainCamera();

        luna::samples::MeshPushConstants push_constants;
        push_constants.m_model = glm::scale(glm::mat4(1.0f), glm::vec3{1.4f, 1.0f, 1.0f});
        push_constants.m_view_projection = luna::samples::buildViewProjection(
            camera,
            static_cast<float>(scene_color.GetWidth()) / static_cast<float>(scene_color.GetHeight()));

        state.Commands.SetRenderArea(scene_color);
        state.Commands.BindVertexBuffers(m_vertex_buffer);
        state.Commands.BindIndexBufferUInt32(m_index_buffer);
        state.Commands.PushConstants(state.Pass, &push_constants);
        state.Commands.DrawIndexed(6, 1);
    }

private:
    static constexpr TextureVertex k_vertices[] = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    };

    static constexpr uint32_t k_indices[] = {0, 1, 2, 2, 3, 0};

private:
    VulkanAbstractionLayer::Format m_surface_format{VulkanAbstractionLayer::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::shared_ptr<VulkanAbstractionLayer::GraphicShader> m_shader;
    VulkanAbstractionLayer::Buffer m_vertex_buffer;
    VulkanAbstractionLayer::Buffer m_index_buffer;
    luna::samples::UploadedTexture m_texture;
};

} // namespace

namespace luna::samples::texture {

std::unique_ptr<VulkanAbstractionLayer::RenderGraph> buildTextureRenderGraph(
    const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
{
    VulkanAbstractionLayer::RenderGraphBuilder builder;
    builder.AddRenderPass("texture", std::make_unique<TextureRenderPass>(build_info))
        .AddRenderPass("imgui", std::make_unique<VulkanAbstractionLayer::ImGuiRenderPass>("scene_color"))
        .SetOutputName("scene_color");
    return builder.Build();
}

} // namespace luna::samples::texture
