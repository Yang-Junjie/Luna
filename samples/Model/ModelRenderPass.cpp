#include "Core/Application.h"
#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/ModelLoader.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/ShaderLoader.h"
#include "samples/Model/ModelRenderPass.h"
#include "Vulkan/GraphicShader.h"
#include "Vulkan/VulkanContext.h"

#include <filesystem>
#include <functional>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <memory>
#include <vector>

namespace {

struct ModelVertex {
    glm::vec3 m_position;
    glm::vec2 m_uv;
    glm::vec3 m_normal;
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

struct ModelMesh {
    luna::val::Buffer m_vertex_buffer;
    luna::val::Buffer m_index_buffer;
    uint32_t m_index_count{0};
    uint32_t m_material_index{std::numeric_limits<uint32_t>::max()};
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

glm::mat4 buildViewProjection(const Camera& camera, float aspect_ratio)
{
    const float clamped_aspect_ratio = std::max(aspect_ratio, 0.001f);
    const glm::mat4 projection = glm::perspective(glm::radians(50.0f), clamped_aspect_ratio, 0.05f, 200.0f);
    return projection * camera.getViewMatrix();
}

UploadedTexture createFallbackTexture()
{
    luna::val::ImageData image_data;
    image_data.ImageFormat = luna::val::Format::R8G8B8A8_UNORM;
    image_data.Width = 1;
    image_data.Height = 1;
    image_data.ByteData = {255, 255, 255, 255};
    return UploadedTexture{
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
        .m_staging_buffer =
            [&image_data]() {
                luna::val::Buffer buffer(image_data.ByteData.size(),
                                         luna::val::BufferUsage::TRANSFER_SOURCE,
                                         luna::val::MemoryUsage::CPU_TO_GPU);
                buffer.CopyData(image_data.ByteData.data(), image_data.ByteData.size(), 0);
                return buffer;
            }(),
    };
}

UploadedTexture createTexture(const luna::val::ImageData& image_data)
{
    const auto& source_image = image_data.ByteData.empty()
                                   ? luna::val::ImageData{std::vector<uint8_t>{255, 255, 255, 255},
                                                          luna::val::Format::R8G8B8A8_UNORM,
                                                          1,
                                                          1,
                                                          {}}
                                   : image_data;

    UploadedTexture texture{
        .m_image = luna::val::Image(source_image.Width,
                                    source_image.Height,
                                    source_image.ImageFormat,
                                    luna::val::ImageUsage::TRANSFER_DISTINATION | luna::val::ImageUsage::SHADER_READ |
                                        luna::val::ImageUsage::TRANSFER_SOURCE,
                                    luna::val::MemoryUsage::GPU_ONLY,
                                    luna::val::ImageOptions::MIPMAPS),
        .m_sampler = luna::val::Sampler(luna::val::Sampler::MinFilter::LINEAR,
                                        luna::val::Sampler::MagFilter::LINEAR,
                                        luna::val::Sampler::AddressMode::REPEAT,
                                        luna::val::Sampler::MipFilter::LINEAR),
        .m_staging_buffer = luna::val::Buffer(
            source_image.ByteData.size(), luna::val::BufferUsage::TRANSFER_SOURCE, luna::val::MemoryUsage::CPU_TO_GPU),
    };

    texture.m_staging_buffer.CopyData(source_image.ByteData.data(), source_image.ByteData.size(), 0);
    return texture;
}

void updateCombinedImageSamplerDescriptor(const vk::DescriptorSet& descriptor_set,
                                          uint32_t binding,
                                          const luna::val::Image& image,
                                          const luna::val::Sampler& sampler)
{
    const vk::DescriptorImageInfo descriptor_image_info{
        sampler.GetNativeHandle(),
        image.GetNativeView(luna::val::ImageView::NATIVE),
        luna::val::ImageUsageToImageLayout(luna::val::ImageUsage::SHADER_READ),
    };

    vk::WriteDescriptorSet write_descriptor_set;
    write_descriptor_set.setDstSet(descriptor_set)
        .setDstBinding(binding)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(1)
        .setPImageInfo(&descriptor_image_info);

    luna::val::GetCurrentVulkanContext().GetDevice().updateDescriptorSets(write_descriptor_set, {});
}

class ModelRenderPass final : public luna::val::RenderPass {
public:
    explicit ModelRenderPass(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
        : m_surface_format(build_info.m_surface_format),
          m_width(build_info.m_framebuffer_width),
          m_height(build_info.m_framebuffer_height),
          m_shader(loadGraphicsShader(projectRoot() / "samples" / "Model" / "Shaders" / "Model.vert",
                                      projectRoot() / "samples" / "Model" / "Shaders" / "Model.frag")),
          m_fallback_texture(createFallbackTexture())
    {
        loadModel();
    }

    void SetupPipeline(luna::val::PipelineState pipeline) override
    {
        pipeline.Shader = m_shader;
        pipeline.VertexBindings = {
            {luna::val::VertexBinding::Rate::PER_VERTEX, luna::val::VertexBinding::BindingRangeAll}};
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.DeclareAttachment("scene_depth", luna::val::Format::D32_SFLOAT, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", luna::val::ClearColor{0.04f, 0.05f, 0.08f, 1.0f});
        pipeline.AddOutputAttachment("scene_depth", luna::val::ClearDepthStencil{1.0f, 0});
    }

    void BeforeRender(luna::val::RenderPassState state) override
    {
        m_fallback_texture.uploadIfNeeded(state.Commands);
        for (auto& texture : m_material_textures) {
            texture.uploadIfNeeded(state.Commands);
        }
    }

    void OnRender(luna::val::RenderPassState state) override
    {
        const auto& scene_color = state.GetAttachment("scene_color");
        const auto& camera = luna::Application::get().getRenderer().getMainCamera();

        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3{m_model_scale});
        model = glm::translate(model, -m_model_center);
        model = glm::rotate(model, static_cast<float>(glfwGetTime()) * 0.4f, glm::vec3{0.0f, 1.0f, 0.0f});

        MeshPushConstants push_constants;
        push_constants.m_model = model;
        push_constants.m_view_projection = buildViewProjection(
            camera, static_cast<float>(scene_color.GetWidth()) / static_cast<float>(scene_color.GetHeight()));

        state.Commands.SetRenderArea(scene_color);
        for (const auto& mesh : m_meshes) {
            const auto& texture = selectTexture(mesh.m_material_index);
            updateCombinedImageSamplerDescriptor(state.Pass.DescriptorSet, 0, texture.m_image, texture.m_sampler);

            state.Commands.BindVertexBuffers(mesh.m_vertex_buffer);
            state.Commands.BindIndexBufferUInt32(mesh.m_index_buffer);
            state.Commands.PushConstants(state.Pass, &push_constants);
            state.Commands.DrawIndexed(mesh.m_index_count, 1);
        }
    }

private:
    void loadModel()
    {
        const auto model_data = luna::val::ModelLoader::Load(
            (projectRoot() / "assets" / "material_sphere" / "material_sphere.obj").string());

        for (const auto& material : model_data.Materials) {
            m_material_textures.push_back(createTexture(material.AlbedoTexture));
        }

        glm::vec3 bounds_min{std::numeric_limits<float>::max()};
        glm::vec3 bounds_max{std::numeric_limits<float>::lowest()};

        for (const auto& shape : model_data.Shapes) {
            if (shape.Vertices.empty() || shape.Indices.empty()) {
                continue;
            }

            std::vector<ModelVertex> vertices;
            vertices.reserve(shape.Vertices.size());
            for (const auto& vertex : shape.Vertices) {
                const glm::vec3 position{vertex.Position.x, vertex.Position.y, vertex.Position.z};
                bounds_min = glm::min(bounds_min, position);
                bounds_max = glm::max(bounds_max, position);
                vertices.push_back(ModelVertex{
                    .m_position = position,
                    .m_uv = glm::vec2{vertex.TexCoord.x, vertex.TexCoord.y},
                    .m_normal = glm::vec3{vertex.Normal.x, vertex.Normal.y, vertex.Normal.z},
                });
            }

            ModelMesh mesh{
                .m_vertex_buffer = luna::val::Buffer(vertices.size() * sizeof(ModelVertex),
                                                     luna::val::BufferUsage::VERTEX_BUFFER,
                                                     luna::val::MemoryUsage::CPU_TO_GPU),
                .m_index_buffer = luna::val::Buffer(shape.Indices.size() * sizeof(luna::val::ModelData::Index),
                                                    luna::val::BufferUsage::INDEX_BUFFER,
                                                    luna::val::MemoryUsage::CPU_TO_GPU),
                .m_index_count = static_cast<uint32_t>(shape.Indices.size()),
                .m_material_index = shape.MaterialIndex,
            };

            mesh.m_vertex_buffer.CopyData(
                reinterpret_cast<const uint8_t*>(vertices.data()), vertices.size() * sizeof(ModelVertex), 0);
            mesh.m_index_buffer.CopyData(reinterpret_cast<const uint8_t*>(shape.Indices.data()),
                                         shape.Indices.size() * sizeof(luna::val::ModelData::Index),
                                         0);
            m_meshes.push_back(std::move(mesh));
        }

        if (m_meshes.empty()) {
            return;
        }

        m_model_center = (bounds_min + bounds_max) * 0.5f;
        const glm::vec3 extent = bounds_max - bounds_min;
        const float longest_axis = std::max(extent.x, std::max(extent.y, extent.z));
        m_model_scale = longest_axis > 0.0f ? 2.0f / longest_axis : 1.0f;
    }

    const UploadedTexture& selectTexture(uint32_t material_index) const
    {
        if (material_index < m_material_textures.size()) {
            return m_material_textures[material_index];
        }
        return m_fallback_texture;
    }

private:
    luna::val::Format m_surface_format{luna::val::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::shared_ptr<luna::val::GraphicShader> m_shader;
    UploadedTexture m_fallback_texture;
    std::vector<UploadedTexture> m_material_textures;
    std::vector<ModelMesh> m_meshes;
    glm::vec3 m_model_center{0.0f};
    float m_model_scale{1.0f};
};

} // namespace

namespace luna::samples::model {

std::unique_ptr<luna::val::RenderGraph>
    buildModelRenderGraph(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
{
    luna::val::RenderGraphBuilder builder;
    builder.AddRenderPass("model", std::make_unique<ModelRenderPass>(build_info))
        .AddRenderPass("imgui", std::make_unique<luna::val::ImGuiRenderPass>("scene_color"))
        .SetOutputName("scene_color");
    return builder.Build();
}

} // namespace luna::samples::model
