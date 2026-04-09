#include "samples/Model/ModelRenderPass.h"

#include "Core/Application.h"
#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/ModelLoader.h"
#include "Renderer/RenderGraphBuilder.h"
#include "samples/Common/SampleCommon.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <limits>
#include <vector>

namespace {

struct ModelVertex {
    glm::vec3 m_position;
    glm::vec2 m_uv;
    glm::vec3 m_normal;
};

struct ModelMesh {
    VulkanAbstractionLayer::Buffer m_vertex_buffer;
    VulkanAbstractionLayer::Buffer m_index_buffer;
    uint32_t m_index_count{0};
    uint32_t m_material_index{std::numeric_limits<uint32_t>::max()};
};

class ModelRenderPass final : public VulkanAbstractionLayer::RenderPass {
public:
    explicit ModelRenderPass(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
        : m_surface_format(build_info.m_surface_format),
          m_width(build_info.m_framebuffer_width),
          m_height(build_info.m_framebuffer_height),
          m_shader(luna::samples::loadGraphicsShader(
              luna::samples::projectRoot() / "samples" / "Model" / "Shaders" / "Model.vert",
              luna::samples::projectRoot() / "samples" / "Model" / "Shaders" / "Model.frag")),
          m_fallback_texture(luna::samples::createTexture({}))
    {
        loadModel();
    }

    void SetupPipeline(VulkanAbstractionLayer::PipelineState pipeline) override
    {
        pipeline.Shader = m_shader;
        pipeline.VertexBindings = {{VulkanAbstractionLayer::VertexBinding::Rate::PER_VERTEX, VulkanAbstractionLayer::VertexBinding::BindingRangeAll}};
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.DeclareAttachment("scene_depth", VulkanAbstractionLayer::Format::D32_SFLOAT, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", VulkanAbstractionLayer::ClearColor{0.04f, 0.05f, 0.08f, 1.0f});
        pipeline.AddOutputAttachment("scene_depth", VulkanAbstractionLayer::ClearDepthStencil{1.0f, 0});
    }

    void BeforeRender(VulkanAbstractionLayer::RenderPassState state) override
    {
        m_fallback_texture.uploadIfNeeded(state.Commands);
        for (auto& texture : m_material_textures) {
            texture.uploadIfNeeded(state.Commands);
        }
    }

    void OnRender(VulkanAbstractionLayer::RenderPassState state) override
    {
        const auto& scene_color = state.GetAttachment("scene_color");
        const auto& camera = luna::Application::get().getRenderer().getMainCamera();

        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3{m_model_scale});
        model = glm::translate(model, -m_model_center);
        model = glm::rotate(model, static_cast<float>(glfwGetTime()) * 0.4f, glm::vec3{0.0f, 1.0f, 0.0f});

        luna::samples::MeshPushConstants push_constants;
        push_constants.m_model = model;
        push_constants.m_view_projection = luna::samples::buildViewProjection(
            camera,
            static_cast<float>(scene_color.GetWidth()) / static_cast<float>(scene_color.GetHeight()),
            50.0f,
            0.05f,
            200.0f);

        state.Commands.SetRenderArea(scene_color);
        for (const auto& mesh : m_meshes) {
            const auto& texture = selectTexture(mesh.m_material_index);
            luna::samples::updateCombinedImageSamplerDescriptor(
                state.Pass.DescriptorSet,
                0,
                texture.m_image,
                texture.m_sampler);

            state.Commands.BindVertexBuffers(mesh.m_vertex_buffer);
            state.Commands.BindIndexBufferUInt32(mesh.m_index_buffer);
            state.Commands.PushConstants(state.Pass, &push_constants);
            state.Commands.DrawIndexed(mesh.m_index_count, 1);
        }
    }

private:
    void loadModel()
    {
        const auto model_data =
            VulkanAbstractionLayer::ModelLoader::Load(luna::samples::assetPath("basicmesh.glb").string());

        for (const auto& material : model_data.Materials) {
            m_material_textures.push_back(luna::samples::createTexture(material.AlbedoTexture));
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
                glm::vec3 position{vertex.Position.x, vertex.Position.y, vertex.Position.z};
                bounds_min = glm::min(bounds_min, position);
                bounds_max = glm::max(bounds_max, position);

                vertices.push_back(ModelVertex{
                    .m_position = position,
                    .m_uv = glm::vec2{vertex.TexCoord.x, vertex.TexCoord.y},
                    .m_normal = glm::vec3{vertex.Normal.x, vertex.Normal.y, vertex.Normal.z},
                });
            }

            ModelMesh mesh{
                .m_vertex_buffer = VulkanAbstractionLayer::Buffer(
                    vertices.size() * sizeof(ModelVertex),
                    VulkanAbstractionLayer::BufferUsage::VERTEX_BUFFER,
                    VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
                .m_index_buffer = VulkanAbstractionLayer::Buffer(
                    shape.Indices.size() * sizeof(VulkanAbstractionLayer::ModelData::Index),
                    VulkanAbstractionLayer::BufferUsage::INDEX_BUFFER,
                    VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
                .m_index_count = static_cast<uint32_t>(shape.Indices.size()),
                .m_material_index = shape.MaterialIndex,
            };

            mesh.m_vertex_buffer.CopyData(
                reinterpret_cast<const uint8_t*>(vertices.data()),
                vertices.size() * sizeof(ModelVertex),
                0);
            mesh.m_index_buffer.CopyData(
                reinterpret_cast<const uint8_t*>(shape.Indices.data()),
                shape.Indices.size() * sizeof(VulkanAbstractionLayer::ModelData::Index),
                0);
            m_meshes.push_back(std::move(mesh));
        }

        if (m_meshes.empty()) {
            m_model_center = glm::vec3{0.0f};
            m_model_scale = 1.0f;
            return;
        }

        m_model_center = (bounds_min + bounds_max) * 0.5f;
        const glm::vec3 extent = bounds_max - bounds_min;
        const float longest_axis = std::max(extent.x, std::max(extent.y, extent.z));
        m_model_scale = longest_axis > 0.0f ? 2.0f / longest_axis : 1.0f;
    }

    const luna::samples::UploadedTexture& selectTexture(uint32_t material_index) const
    {
        if (material_index < m_material_textures.size()) {
            return m_material_textures[material_index];
        }
        return m_fallback_texture;
    }

private:
    VulkanAbstractionLayer::Format m_surface_format{VulkanAbstractionLayer::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::shared_ptr<VulkanAbstractionLayer::GraphicShader> m_shader;
    luna::samples::UploadedTexture m_fallback_texture;
    std::vector<luna::samples::UploadedTexture> m_material_textures;
    std::vector<ModelMesh> m_meshes;
    glm::vec3 m_model_center{0.0f};
    float m_model_scale{1.0f};
};

} // namespace

namespace luna::samples::model {

std::unique_ptr<VulkanAbstractionLayer::RenderGraph> buildModelRenderGraph(
    const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
{
    VulkanAbstractionLayer::RenderGraphBuilder builder;
    builder.AddRenderPass("model", std::make_unique<ModelRenderPass>(build_info))
        .AddRenderPass("imgui", std::make_unique<VulkanAbstractionLayer::ImGuiRenderPass>("scene_color"))
        .SetOutputName("scene_color");
    return builder.Build();
}

} // namespace luna::samples::model
