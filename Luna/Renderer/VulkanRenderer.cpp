#include "Core/Log.h"
#include "Core/Window.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/VulkanRenderer.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanSurface.h"

#include <algorithm>
#include <GLFW/glfw3.h>
#include <vector>

namespace {

using RenderGraphBuildInfo = luna::VulkanRenderer::RenderGraphBuildInfo;

std::unique_ptr<luna::val::RenderGraph> buildDefaultRenderGraph(const RenderGraphBuildInfo& build_info,
                                                                luna::SceneRenderer& scene_renderer,
                                                                bool include_imgui_pass)
{
    return scene_renderer.buildRenderGraph(
        build_info.m_surface_format, build_info.m_framebuffer_width, build_info.m_framebuffer_height, include_imgui_pass);
}

} // namespace

namespace luna {

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer()
{
    shutdown();
}

bool VulkanRenderer::init(Window& window, InitializationOptions options)
{
    shutdown();

    m_window = &window;
    m_native_window = static_cast<GLFWwindow*>(window.getNativeWindow());
    m_initialization_options = std::move(options);
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

    luna::val::VulkanContextCreateOptions create_options{};
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

    m_context = std::make_unique<luna::val::VulkanContext>(create_options);
    luna::val::SetCurrentVulkanContext(*m_context);

    luna::val::ContextInitializeOptions init_options{};
    init_options.VirtualFrameCount = 2;
    init_options.ErrorCallback = create_options.ErrorCallback;
    init_options.InfoCallback = create_options.InfoCallback;

    const auto& surface = luna::val::CreateWindowSurface(m_native_window, *m_context);
    m_context->InitializeContext(surface, init_options);

    if (!m_context->IsRenderingEnabled()) {
        LUNA_CORE_ERROR("luna::val failed to initialize rendering");
        shutdown();
        return false;
    }

    m_main_camera.m_position = glm::vec3(0.0f, 0.0f, 5.0f);
    rebuildRenderGraph();

    m_initialized = true;
    m_resize_requested = false;
    m_render_graph_rebuild_requested = false;
    m_frame_started = false;
    return true;
}

void VulkanRenderer::shutdown()
{
    if (!m_initialized) {
        return;
    }
    m_render_graph.reset();
    m_scene_renderer.shutdown();
    m_context.reset();
    m_window = nullptr;
    m_native_window = nullptr;
    m_initialization_options = {};
    m_resize_requested = false;
    m_render_graph_rebuild_requested = false;
    m_frame_started = false;
    m_initialized = false;
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

bool VulkanRenderer::isImGuiEnabled() const
{
    return m_imgui_enabled;
}

void VulkanRenderer::requestResize()
{
    m_resize_requested = true;
}

void VulkanRenderer::requestRenderGraphRebuild()
{
    m_render_graph_rebuild_requested = true;
}

bool VulkanRenderer::isResizeRequested() const
{
    return m_resize_requested;
}

void VulkanRenderer::setImGuiEnabled(bool enabled)
{
    if (m_imgui_enabled == enabled) {
        return;
    }

    m_imgui_enabled = enabled;
    if (m_initialized) {
        rebuildRenderGraph();
    }
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

    if (m_render_graph_rebuild_requested) {
        rebuildRenderGraph();
        m_render_graph_rebuild_requested = false;
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
    m_render_graph->Present(commands,
                            m_context->AcquireCurrentSwapchainImage(luna::val::ImageUsage::TRANSFER_DISTINATION));
}

void VulkanRenderer::endFrame()
{
    if (!m_frame_started) {
        return;
    }

    m_context->EndFrame();
    m_scene_renderer.clearSubmittedMeshes();
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

SceneRenderer& VulkanRenderer::getSceneRenderer()
{
    return m_scene_renderer;
}

const SceneRenderer& VulkanRenderer::getSceneRenderer() const
{
    return m_scene_renderer;
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

    const RenderGraphBuildInfo build_info{
        .m_surface_format = m_context->GetSurfaceFormat(),
        .m_framebuffer_width = extent.width,
        .m_framebuffer_height = extent.height,
    };

    if (m_initialization_options.m_render_graph_builder) {
        m_render_graph = m_initialization_options.m_render_graph_builder(build_info);
    } else {
        m_render_graph = buildDefaultRenderGraph(build_info, m_scene_renderer, m_imgui_enabled);
    }
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

    (void) m_context->GetDevice().waitIdle();
    m_context->RecreateSwapchain(extent.width, extent.height);
    rebuildRenderGraph();
    m_resize_requested = false;
}

} // namespace luna
