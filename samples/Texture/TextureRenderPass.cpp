#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/ImageLoader.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/ShaderLoader.h"
#include "samples/Texture/TextureRenderPass.h"
#include "Vulkan/GraphicShader.h"

#include <filesystem>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
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
    luna::val::Image m_image;
    luna::val::Sampler m_sampler;
    luna::val::Buffer m_staging_buffer;
    bool m_uploaded{false};

    void uploadIfNeeded(luna::val::CommandBuffer& commands)
    {
        if (m_uploaded) {
            return;
        }

        commands.CopyBufferToImage(luna::val::BufferInfo{std::cref(m_staging_buffer), 0},
                                   luna::val::ImageInfo{std::cref(m_image), luna::val::ImageUsage::UNKNOWN, 0, 0});

        if (m_image.GetMipLevelCount() > 1) {
            commands.GenerateMipLevels(
                m_image, luna::val::ImageUsage::TRANSFER_DISTINATION, luna::val::BlitFilter::LINEAR);
        }

        commands.TransferLayout(
            m_image, luna::val::ImageUsage::TRANSFER_DISTINATION, luna::val::ImageUsage::SHADER_READ);
        m_uploaded = true;
    }
};

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

std::shared_ptr<luna::val::GraphicShader> loadGraphicsShader(const std::filesystem::path& vertex_path,
                                                             const std::filesystem::path& fragment_path)
{
    auto shader = std::make_shared<luna::val::GraphicShader>();
    shader->Init(luna::val::ShaderLoader::LoadFromSourceFile(
                     vertex_path.string(), luna::val::ShaderType::VERTEX, luna::val::ShaderLanguage::GLSL),
                 luna::val::ShaderLoader::LoadFromSourceFile(
                     fragment_path.string(), luna::val::ShaderType::FRAGMENT, luna::val::ShaderLanguage::GLSL));
    return shader;
}

UploadedTexture createTexture(const luna::val::ImageData& image_data)
{
    UploadedTexture texture{
        .m_image = luna::val::Image(image_data.Width,
                                    image_data.Height,
                                    image_data.ImageFormat,
                                    luna::val::ImageUsage::TRANSFER_DISTINATION | luna::val::ImageUsage::SHADER_READ |
                                        luna::val::ImageUsage::TRANSFER_SOURCE,
                                    luna::val::MemoryUsage::GPU_ONLY,
                                    luna::val::ImageOptions::MIPMAPS),
        .m_sampler = luna::val::Sampler(luna::val::Sampler::MinFilter::LINEAR,
                                        luna::val::Sampler::MagFilter::LINEAR,
                                        luna::val::Sampler::AddressMode::REPEAT,
                                        luna::val::Sampler::MipFilter::LINEAR),
        .m_staging_buffer = luna::val::Buffer(
            image_data.ByteData.size(), luna::val::BufferUsage::TRANSFER_SOURCE, luna::val::MemoryUsage::CPU_TO_GPU),
    };

    texture.m_staging_buffer.CopyData(image_data.ByteData.data(), image_data.ByteData.size(), 0);
    return texture;
}

class TextureRenderPass final : public luna::val::RenderPass {
public:
    explicit TextureRenderPass(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
        : m_surface_format(build_info.m_surface_format),
          m_width(build_info.m_framebuffer_width),
          m_height(build_info.m_framebuffer_height),
          m_shader(loadGraphicsShader(projectRoot() / "samples" / "Texture" / "Shaders" / "TexturedQuad.vert",
                                      projectRoot() / "samples" / "Texture" / "Shaders" / "TexturedQuad.frag")),
          m_vertex_buffer(
              sizeof(k_vertices), luna::val::BufferUsage::VERTEX_BUFFER, luna::val::MemoryUsage::CPU_TO_GPU),
          m_index_buffer(sizeof(k_indices), luna::val::BufferUsage::INDEX_BUFFER, luna::val::MemoryUsage::CPU_TO_GPU),
          m_texture(createTexture(
              luna::val::ImageLoader::LoadImageFromFile((projectRoot() / "assets" / "head.jpg").string())))
    {
        m_vertex_buffer.CopyData(reinterpret_cast<const uint8_t*>(k_vertices), sizeof(k_vertices), 0);
        m_index_buffer.CopyData(reinterpret_cast<const uint8_t*>(k_indices), sizeof(k_indices), 0);
    }

    void SetupPipeline(luna::val::PipelineState pipeline) override
    {
        pipeline.Shader = m_shader;
        pipeline.VertexBindings = {
            {luna::val::VertexBinding::Rate::PER_VERTEX, luna::val::VertexBinding::BindingRangeAll}};
        pipeline.DescriptorBindings.Bind(
            0, "albedo_texture", m_texture.m_sampler, luna::val::UniformType::COMBINED_IMAGE_SAMPLER);
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", luna::val::ClearColor{0.05f, 0.07f, 0.10f, 1.0f});
    }

    void ResolveResources(luna::val::ResolveState resolve) override
    {
        resolve.Resolve("albedo_texture", m_texture.m_image);
    }

    void BeforeRender(luna::val::RenderPassState state) override
    {
        m_texture.uploadIfNeeded(state.Commands);
    }

    void OnRender(luna::val::RenderPassState state) override
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
    luna::val::Format m_surface_format{luna::val::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::shared_ptr<luna::val::GraphicShader> m_shader;
    luna::val::Buffer m_vertex_buffer;
    luna::val::Buffer m_index_buffer;
    UploadedTexture m_texture;
};

} // namespace

namespace luna::samples::texture {

std::unique_ptr<luna::val::RenderGraph>
    buildTextureRenderGraph(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
{
    luna::val::RenderGraphBuilder builder;
    builder.AddRenderPass("texture", std::make_unique<TextureRenderPass>(build_info))
        .AddRenderPass("imgui", std::make_unique<luna::val::ImGuiRenderPass>("scene_color"))
        .SetOutputName("scene_color");
    return builder.Build();
}

} // namespace luna::samples::texture
