#include "Renderer/VulkanRenderer.h"

#include "Core/Log.h"
#include "Core/Window.h"

#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanSurface.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <vector>

namespace {

class ClearColorPass final : public VulkanAbstractionLayer::RenderPass {
public:
    ClearColorPass(VulkanAbstractionLayer::Format format, uint32_t width, uint32_t height, const glm::vec4* clear_color)
        : m_format(format),
          m_width(width),
          m_height(height),
          m_clear_color(clear_color)
    {}

    void SetupPipeline(VulkanAbstractionLayer::PipelineState pipeline) override
    {
        pipeline.DeclareAttachment("scene_color", m_format, m_width, m_height);
        pipeline.AddOutputAttachment(
            "scene_color",
            VulkanAbstractionLayer::ClearColor{
                m_clear_color != nullptr ? m_clear_color->x : 0.0f,
                m_clear_color != nullptr ? m_clear_color->y : 0.0f,
                m_clear_color != nullptr ? m_clear_color->z : 0.0f,
                m_clear_color != nullptr ? m_clear_color->w : 1.0f});
    }

private:
    VulkanAbstractionLayer::Format m_format{VulkanAbstractionLayer::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
    const glm::vec4* m_clear_color{nullptr};
};

} // namespace

namespace luna {

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer()
{
    shutdown();
}

bool VulkanRenderer::init(Window& window)
{
    shutdown();

    m_window = &window;
    m_native_window = static_cast<GLFWwindow*>(window.getNativeWindow());
    if (m_native_window == nullptr) {
        LUNA_CORE_ERROR("Cannot initialize Vulkan renderer without a GLFW window");
        return false;
    }

    uint32_t extension_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
    if (extensions == nullptr || extension_count == 0) {
        LUNA_CORE_ERROR("Failed to query required GLFW Vulkan extensions");
        return false;
    }

    std::vector<const char*> required_extensions(extensions, extensions + extension_count);

    VulkanAbstractionLayer::VulkanContextCreateOptions create_options{};
    create_options.VulkanApiMajorVersion = 1;
    create_options.VulkanApiMinorVersion = 3;
    create_options.Extensions = required_extensions;
#ifndef NDEBUG
    create_options.Layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    create_options.ErrorCallback = [](const std::string& message) {
        LUNA_CORE_ERROR("VAL: {}", message);
    };
    create_options.InfoCallback = [](const std::string& message) {
        LUNA_CORE_INFO("VAL: {}", message);
    };
    create_options.ApplicationName = "Luna";
    create_options.EngineName = "Luna";

    m_context = std::make_unique<VulkanAbstractionLayer::VulkanContext>(create_options);
    VulkanAbstractionLayer::SetCurrentVulkanContext(*m_context);

    VulkanAbstractionLayer::ContextInitializeOptions init_options{};
    init_options.VirtualFrameCount = 2;
    init_options.ErrorCallback = create_options.ErrorCallback;
    init_options.InfoCallback = create_options.InfoCallback;

    const auto& surface = VulkanAbstractionLayer::CreateWindowSurface(m_native_window, *m_context);
    m_context->InitializeContext(surface, init_options);

    if (!m_context->IsRenderingEnabled()) {
        LUNA_CORE_ERROR("VulkanAbstractionLayer failed to initialize rendering");
        shutdown();
        return false;
    }

    m_main_camera.m_position = glm::vec3(0.0f, 0.0f, 5.0f);
    rebuildRenderGraph();

    m_initialized = true;
    m_resize_requested = false;
    m_frame_started = false;
    return true;
}

void VulkanRenderer::shutdown()
{
    m_render_graph.reset();
    m_context.reset();
    m_window = nullptr;
    m_native_window = nullptr;
    m_initialized = false;
    m_resize_requested = false;
    m_frame_started = false;
}

bool VulkanRenderer::isInitialized() const
{
    return m_initialized && m_context != nullptr;
}

bool VulkanRenderer::isRenderingEnabled() const
{
    const vk::Extent2D extent = getFramebufferExtent();
    return isInitialized() && m_context->IsRenderingEnabled() && extent.width > 0 && extent.height > 0;
}

void VulkanRenderer::requestResize()
{
    m_resize_requested = true;
}

bool VulkanRenderer::isResizeRequested() const
{
    return m_resize_requested;
}

void VulkanRenderer::startFrame()
{
    if (!isRenderingEnabled()) {
        return;
    }

    handlePendingResize();
    if (!isRenderingEnabled()) {
        return;
    }

    m_context->StartFrame();
    m_frame_started = true;
}

void VulkanRenderer::renderFrame()
{
    if (!m_frame_started || m_render_graph == nullptr) {
        return;
    }

    auto& commands = m_context->GetCurrentCommandBuffer();
    m_render_graph->Execute(commands);
    m_render_graph->Present(
        commands, m_context->AcquireCurrentSwapchainImage(VulkanAbstractionLayer::ImageUsage::TRANSFER_DISTINATION));
}

void VulkanRenderer::endFrame()
{
    if (!m_frame_started) {
        return;
    }

    m_context->EndFrame();
    m_frame_started = false;
}

GLFWwindow* VulkanRenderer::getNativeWindow() const
{
    return m_native_window;
}

const vk::RenderPass& VulkanRenderer::getImGuiRenderPass() const
{
    return m_render_graph->GetNodeByName("imgui").PassNative.RenderPassHandle;
}

Camera& VulkanRenderer::getMainCamera()
{
    return m_main_camera;
}

const Camera& VulkanRenderer::getMainCamera() const
{
    return m_main_camera;
}

glm::vec4& VulkanRenderer::getClearColor()
{
    return m_clear_color;
}

const glm::vec4& VulkanRenderer::getClearColor() const
{
    return m_clear_color;
}

void VulkanRenderer::rebuildRenderGraph()
{
    if (m_context == nullptr) {
        return;
    }

    const vk::Extent2D extent = getFramebufferExtent();
    if (extent.width == 0 || extent.height == 0) {
        m_render_graph.reset();
        return;
    }

    VulkanAbstractionLayer::RenderGraphBuilder builder;
    builder.AddRenderPass(
               "clear",
               std::make_unique<ClearColorPass>(m_context->GetSurfaceFormat(), extent.width, extent.height, &m_clear_color))
        .AddRenderPass("imgui", std::make_unique<VulkanAbstractionLayer::ImGuiRenderPass>("scene_color"))
        .SetOutputName("scene_color");

    m_render_graph = builder.Build();
}

vk::Extent2D VulkanRenderer::getFramebufferExtent() const
{
    if (m_native_window == nullptr) {
        return {};
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_native_window, &width, &height);
    return {static_cast<uint32_t>(std::max(width, 0)), static_cast<uint32_t>(std::max(height, 0))};
}

void VulkanRenderer::handlePendingResize()
{
    if (!m_resize_requested || m_context == nullptr) {
        return;
    }

    const vk::Extent2D extent = getFramebufferExtent();
    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    (void)m_context->GetDevice().waitIdle();
    m_context->RecreateSwapchain(extent.width, extent.height);
    rebuildRenderGraph();
    m_resize_requested = false;
}

} // namespace luna
