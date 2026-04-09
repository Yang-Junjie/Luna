#include "samples/Texture/TextureRenderPass.h"

#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/ImageLoader.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/ShaderLoader.h"
#include "Vulkan/GraphicShader.h"

#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <functional>
#include <memory>

namespace {

struct TextureVertex {
    glm::vec3 m_position;
    glm::vec2 m_uv;
};

struct MeshPushConstants {
    glm::mat4 m_model{1.0f};
    glm::mat4 m_view_projection{1.0f};
};

struct UploadedTexture {
    VulkanAbstractionLayer::Image m_image;
    VulkanAbstractionLayer::Sampler m_sampler;
    VulkanAbstractionLayer::Buffer m_staging_buffer;
    bool m_uploaded{false};

    void uploadIfNeeded(VulkanAbstractionLayer::CommandBuffer& commands)
    {
        if (m_uploaded) {
            return;
        }

        commands.CopyBufferToImage(
            VulkanAbstractionLayer::BufferInfo{std::cref(m_staging_buffer), 0},
            VulkanAbstractionLayer::ImageInfo{std::cref(m_image), VulkanAbstractionLayer::ImageUsage::UNKNOWN, 0, 0});

        if (m_image.GetMipLevelCount() > 1) {
            commands.GenerateMipLevels(
                m_image,
                VulkanAbstractionLayer::ImageUsage::TRANSFER_DISTINATION,
                VulkanAbstractionLayer::BlitFilter::LINEAR);
        }

        commands.TransferLayout(
            m_image,
            VulkanAbstractionLayer::ImageUsage::TRANSFER_DISTINATION,
            VulkanAbstractionLayer::ImageUsage::SHADER_READ);
        m_uploaded = true;
    }
};

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

std::shared_ptr<VulkanAbstractionLayer::GraphicShader> loadGraphicsShader(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path)
{
    auto shader = std::make_shared<VulkanAbstractionLayer::GraphicShader>();
    shader->Init(
        VulkanAbstractionLayer::ShaderLoader::LoadFromSourceFile(
            vertex_path.string(),
            VulkanAbstractionLayer::ShaderType::VERTEX,
            VulkanAbstractionLayer::ShaderLanguage::GLSL),
        VulkanAbstractionLayer::ShaderLoader::LoadFromSourceFile(
            fragment_path.string(),
            VulkanAbstractionLayer::ShaderType::FRAGMENT,
            VulkanAbstractionLayer::ShaderLanguage::GLSL));
    return shader;
}

UploadedTexture createTexture(const VulkanAbstractionLayer::ImageData& image_data)
{
    UploadedTexture texture{
        .m_image = VulkanAbstractionLayer::Image(
            image_data.Width,
            image_data.Height,
            image_data.ImageFormat,
            VulkanAbstractionLayer::ImageUsage::TRANSFER_DISTINATION |
                VulkanAbstractionLayer::ImageUsage::SHADER_READ |
                VulkanAbstractionLayer::ImageUsage::TRANSFER_SOURCE,
            VulkanAbstractionLayer::MemoryUsage::GPU_ONLY,
            VulkanAbstractionLayer::ImageOptions::MIPMAPS),
        .m_sampler = VulkanAbstractionLayer::Sampler(
            VulkanAbstractionLayer::Sampler::MinFilter::LINEAR,
            VulkanAbstractionLayer::Sampler::MagFilter::LINEAR,
            VulkanAbstractionLayer::Sampler::AddressMode::REPEAT,
            VulkanAbstractionLayer::Sampler::MipFilter::LINEAR),
        .m_staging_buffer = VulkanAbstractionLayer::Buffer(
            image_data.ByteData.size(),
            VulkanAbstractionLayer::BufferUsage::TRANSFER_SOURCE,
            VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
    };

    texture.m_staging_buffer.CopyData(image_data.ByteData.data(), image_data.ByteData.size(), 0);
    return texture;
}

class TextureRenderPass final : public VulkanAbstractionLayer::RenderPass {
public:
    explicit TextureRenderPass(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
        : m_surface_format(build_info.m_surface_format),
          m_width(build_info.m_framebuffer_width),
          m_height(build_info.m_framebuffer_height),
          m_shader(loadGraphicsShader(
              projectRoot() / "samples" / "Texture" / "Shaders" / "TexturedQuad.vert",
              projectRoot() / "samples" / "Texture" / "Shaders" / "TexturedQuad.frag")),
          m_vertex_buffer(
              sizeof(k_vertices),
              VulkanAbstractionLayer::BufferUsage::VERTEX_BUFFER,
              VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
          m_index_buffer(
              sizeof(k_indices),
              VulkanAbstractionLayer::BufferUsage::INDEX_BUFFER,
              VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
          m_texture(createTexture(
              VulkanAbstractionLayer::ImageLoader::LoadImageFromFile((projectRoot() / "assets" / "head.jpg").string())))
    {
        m_vertex_buffer.CopyData(reinterpret_cast<const uint8_t*>(k_vertices), sizeof(k_vertices), 0);
        m_index_buffer.CopyData(reinterpret_cast<const uint8_t*>(k_indices), sizeof(k_indices), 0);
    }

    void SetupPipeline(VulkanAbstractionLayer::PipelineState pipeline) override
    {
        pipeline.Shader = m_shader;
        pipeline.VertexBindings = {
            {VulkanAbstractionLayer::VertexBinding::Rate::PER_VERTEX, VulkanAbstractionLayer::VertexBinding::BindingRangeAll}};
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

        MeshPushConstants push_constants;
        push_constants.m_model = glm::scale(glm::mat4(1.0f), glm::vec3{1.4f, 1.0f, 1.0f});
        push_constants.m_view_projection = glm::ortho(-1.6f, 1.6f, -0.9f, 0.9f, -1.0f, 1.0f);

        state.Commands.SetRenderArea(scene_color);
        state.Commands.BindVertexBuffers(m_vertex_buffer);
        state.Commands.BindIndexBufferUInt32(m_index_buffer);
        state.Commands.PushConstants(state.Pass, &push_constants);
        state.Commands.DrawIndexed(6, 1);
    }

private:
    static constexpr TextureVertex k_vertices[] = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    };

    static constexpr uint32_t k_indices[] = {0, 1, 2, 2, 3, 0};

private:
    VulkanAbstractionLayer::Format m_surface_format{VulkanAbstractionLayer::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::shared_ptr<VulkanAbstractionLayer::GraphicShader> m_shader;
    VulkanAbstractionLayer::Buffer m_vertex_buffer;
    VulkanAbstractionLayer::Buffer m_index_buffer;
    UploadedTexture m_texture;
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
