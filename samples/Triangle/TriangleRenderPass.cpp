#include "samples/Triangle/TriangleRenderPass.h"

#include "Core/Application.h"
#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/RenderGraphBuilder.h"
#include "samples/Common/SampleCommon.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {

struct TriangleVertex {
    glm::vec3 m_position;
    glm::vec3 m_color;
};

class TriangleRenderPass final : public VulkanAbstractionLayer::RenderPass {
public:
    explicit TriangleRenderPass(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
        : m_surface_format(build_info.m_surface_format),
          m_width(build_info.m_framebuffer_width),
          m_height(build_info.m_framebuffer_height),
          m_shader(luna::samples::loadGraphicsShader(
              luna::samples::projectRoot() / "samples" / "Triangle" / "Shaders" / "Triangle.vert",
              luna::samples::projectRoot() / "samples" / "Triangle" / "Shaders" / "Triangle.frag")),
          m_vertex_buffer(
              sizeof(k_vertices),
              VulkanAbstractionLayer::BufferUsage::VERTEX_BUFFER,
              VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU)
    {
        m_vertex_buffer.CopyData(reinterpret_cast<const uint8_t*>(k_vertices), sizeof(k_vertices), 0);
    }

    void SetupPipeline(VulkanAbstractionLayer::PipelineState pipeline) override
    {
        pipeline.Shader = m_shader;
        pipeline.VertexBindings = {{VulkanAbstractionLayer::VertexBinding::Rate::PER_VERTEX, VulkanAbstractionLayer::VertexBinding::BindingRangeAll}};
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", VulkanAbstractionLayer::ClearColor{0.08f, 0.10f, 0.14f, 1.0f});
    }

    void OnRender(VulkanAbstractionLayer::RenderPassState state) override
    {
        const auto& scene_color = state.GetAttachment("scene_color");
        const auto& camera = luna::Application::get().getRenderer().getMainCamera();

        luna::samples::MeshPushConstants push_constants;
        push_constants.m_model = glm::rotate(glm::mat4(1.0f), static_cast<float>(glfwGetTime()) * 0.7f, glm::vec3{0.0f, 1.0f, 0.0f});
        push_constants.m_view_projection = luna::samples::buildViewProjection(
            camera,
            static_cast<float>(scene_color.GetWidth()) / static_cast<float>(scene_color.GetHeight()));

        state.Commands.SetRenderArea(scene_color);
        state.Commands.BindVertexBuffers(m_vertex_buffer);
        state.Commands.PushConstants(state.Pass, &push_constants);
        state.Commands.Draw(3, 1);
    }

private:
    static constexpr TriangleVertex k_vertices[] = {
        {{0.0f, 0.75f, 0.0f}, {1.0f, 0.3f, 0.2f}},
        {{0.7f, -0.55f, 0.0f}, {0.2f, 1.0f, 0.4f}},
        {{-0.7f, -0.55f, 0.0f}, {0.2f, 0.4f, 1.0f}},
    };

private:
    VulkanAbstractionLayer::Format m_surface_format{VulkanAbstractionLayer::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::shared_ptr<VulkanAbstractionLayer::GraphicShader> m_shader;
    VulkanAbstractionLayer::Buffer m_vertex_buffer;
};

} // namespace

namespace luna::samples::triangle {

std::unique_ptr<VulkanAbstractionLayer::RenderGraph> buildTriangleRenderGraph(
    const luna::VulkanRenderer::RenderGraphBuildInfo& build_info)
{
    VulkanAbstractionLayer::RenderGraphBuilder builder;
    builder.AddRenderPass("triangle", std::make_unique<TriangleRenderPass>(build_info))
        .AddRenderPass("imgui", std::make_unique<VulkanAbstractionLayer::ImGuiRenderPass>("scene_color"))
        .SetOutputName("scene_color");
    return builder.Build();
}

} // namespace luna::samples::triangle
