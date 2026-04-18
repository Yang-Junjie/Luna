#include "Core/Log.h"
#include "Core/Window.h"
#include "Imgui/ImGuiContext.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/Renderer.h"

#include <Adapter.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <Device.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <array>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Instance.h>
#include <Queue.h>
#include <stdexcept>
#include <string>
#include <Surface.h>
#include <Swapchain.h>
#include <Synchronization.h>
#include <Texture.h>

namespace {

luna::RHI::Ref<luna::RHI::Adapter> selectAdapter(const std::vector<luna::RHI::Ref<luna::RHI::Adapter>>& adapters)
{
    if (adapters.empty()) {
        return {};
    }

    const auto discrete_adapter =
        std::find_if(adapters.begin(), adapters.end(), [](const luna::RHI::Ref<luna::RHI::Adapter>& adapter) {
            return adapter && adapter->GetAdapterType() == luna::RHI::AdapterType::Discrete;
        });
    return discrete_adapter != adapters.end() ? *discrete_adapter : adapters.front();
}

luna::RHI::SurfaceFormat chooseSurfaceFormat(const std::vector<luna::RHI::SurfaceFormat>& formats)
{
    const auto preferred = std::find_if(formats.begin(), formats.end(), [](const luna::RHI::SurfaceFormat& format) {
        return format.format == luna::RHI::Format::BGRA8_UNORM && format.colorSpace == luna::RHI::ColorSpace::SRGB_NONLINEAR;
    });
    if (preferred != formats.end()) {
        return *preferred;
    }

    const auto fallback = std::find_if(formats.begin(), formats.end(), [](const luna::RHI::SurfaceFormat& format) {
        return format.format == luna::RHI::Format::RGBA8_UNORM || format.format == luna::RHI::Format::BGRA8_UNORM;
    });
    if (fallback != formats.end()) {
        return *fallback;
    }

    return formats.empty() ? luna::RHI::SurfaceFormat{luna::RHI::Format::BGRA8_UNORM, luna::RHI::ColorSpace::SRGB_NONLINEAR}
                           : formats.front();
}

const char* presentModeToString(luna::RHI::PresentMode mode)
{
    switch (mode) {
        case luna::RHI::PresentMode::Immediate:
            return "Immediate";
        case luna::RHI::PresentMode::Mailbox:
            return "Mailbox";
        case luna::RHI::PresentMode::Fifo:
            return "Fifo";
        case luna::RHI::PresentMode::FifoRelaxed:
            return "FifoRelaxed";
        default:
            return "Unknown";
    }
}

const char* backendTypeToString(luna::RHI::BackendType type)
{
    switch (type) {
        case luna::RHI::BackendType::Auto:
            return "Auto";
        case luna::RHI::BackendType::Vulkan:
            return "Vulkan";
        case luna::RHI::BackendType::DirectX12:
            return "DirectX12";
        case luna::RHI::BackendType::DirectX11:
            return "DirectX11";
        case luna::RHI::BackendType::Metal:
            return "Metal";
        case luna::RHI::BackendType::OpenGL:
            return "OpenGL";
        case luna::RHI::BackendType::OpenGLES:
            return "OpenGLES";
        case luna::RHI::BackendType::WebGPU:
            return "WebGPU";
        default:
            return "Unknown";
    }
}

bool usesSceneRenderer(luna::RHI::BackendType type)
{
    return type == luna::RHI::BackendType::Vulkan || type == luna::RHI::BackendType::DirectX11 ||
           type == luna::RHI::BackendType::DirectX12;
}

bool isPresentModeSupported(const std::vector<luna::RHI::PresentMode>& supported_modes, luna::RHI::PresentMode mode)
{
    return std::find(supported_modes.begin(), supported_modes.end(), mode) != supported_modes.end();
}

std::string describePresentModes(const std::vector<luna::RHI::PresentMode>& supported_modes)
{
    if (supported_modes.empty()) {
        return "<none>";
    }

    std::string result;
    for (size_t i = 0; i < supported_modes.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += presentModeToString(supported_modes[i]);
    }
    return result;
}

luna::RHI::PresentMode choosePresentMode(const std::vector<luna::RHI::PresentMode>& supported_modes,
                                     luna::RHI::PresentMode requested_mode)
{
    if (isPresentModeSupported(supported_modes, requested_mode)) {
        return requested_mode;
    }

    switch (requested_mode) {
        case luna::RHI::PresentMode::Mailbox:
            for (const auto fallback_mode :
                 std::array{luna::RHI::PresentMode::Immediate, luna::RHI::PresentMode::FifoRelaxed, luna::RHI::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case luna::RHI::PresentMode::Immediate:
            for (const auto fallback_mode :
                 std::array{luna::RHI::PresentMode::Mailbox, luna::RHI::PresentMode::FifoRelaxed, luna::RHI::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case luna::RHI::PresentMode::FifoRelaxed:
            for (const auto fallback_mode :
                 std::array{luna::RHI::PresentMode::Fifo, luna::RHI::PresentMode::Immediate, luna::RHI::PresentMode::Mailbox}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case luna::RHI::PresentMode::Fifo:
            if (isPresentModeSupported(supported_modes, luna::RHI::PresentMode::FifoRelaxed)) {
                return luna::RHI::PresentMode::FifoRelaxed;
            }
            break;
        default:
            break;
    }

    return supported_modes.empty() ? luna::RHI::PresentMode::Fifo : supported_modes.front();
}

} // namespace

namespace luna {

Renderer::Renderer() = default;

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(Window& window, InitializationOptions options)
{
    shutdown();

    m_window = &window;
    m_native_window = static_cast<GLFWwindow*>(window.getNativeWindow());
    m_initialization_options = std::move(options);
    if (m_native_window == nullptr) {
        LUNA_RENDERER_ERROR("Cannot initialize renderer without a GLFW window");
        return false;
    }

    luna::RHI::NativeWindowHandle window_handle;
    window_handle.hWnd = glfwGetWin32Window(m_native_window);
    window_handle.hInst = GetModuleHandleW(nullptr);

    try {
        luna::RHI::InstanceCreateInfo instance_info;
        instance_info.type = m_initialization_options.backend;
        instance_info.applicationName = "Luna";
        instance_info.enabledFeatures.push_back(luna::RHI::InstanceFeature::Surface);
#ifndef NDEBUG
        instance_info.enabledFeatures.push_back(luna::RHI::InstanceFeature::ValidationLayer);
#endif

        m_instance = luna::RHI::Instance::Create(instance_info);
        m_shader_compiler = m_instance->CreateShaderCompiler();
        m_surface = m_instance->CreateSurface(window_handle);
        if (!m_surface) {
            throw std::runtime_error("Failed to create surface for backend '" +
                                     std::string(backendTypeToString(m_instance->GetType())) + "'");
        }

        const auto adapters = m_instance->EnumerateAdapters();
        m_adapter = selectAdapter(adapters);
        if (!m_adapter) {
            throw std::runtime_error("No compatible adapter available for backend '" +
                                     std::string(backendTypeToString(m_instance->GetType())) + "'");
        }

        luna::RHI::DeviceCreateInfo device_info;
        device_info.QueueRequests = {{luna::RHI::QueueType::Graphics, 1, 1.0f}};
        device_info.CompatibleSurface = m_surface;
        if (m_adapter->IsFeatureSupported(luna::RHI::DeviceFeature::SamplerAnisotropy)) {
            device_info.EnabledFeatures.push_back(luna::RHI::DeviceFeature::SamplerAnisotropy);
        }

        m_device = m_adapter->CreateDevice(device_info);
        m_graphics_queue = m_device->GetQueue(luna::RHI::QueueType::Graphics, 0);

        const auto extent = getFramebufferExtent();
        createSwapchain(extent.width, extent.height);
        LUNA_RENDERER_INFO("Initialized renderer backend '{}'", backendTypeToString(m_instance->GetType()));
    } catch (const std::exception& error) {
        LUNA_RENDERER_ERROR("Failed to initialize Renderer: {}", error.what());
        shutdown();
        return false;
    }

    m_main_camera.m_position = glm::vec3(0.0f, 0.0f, 5.0f);
    m_initialized =
        m_instance && m_adapter && m_device && m_surface && m_swapchain && m_graphics_queue && m_synchronization;
    m_resize_requested = false;
    m_frame_started = false;
    return m_initialized;
}

void Renderer::shutdown()
{
    if (m_device) {
        try {
            if (m_graphics_queue) {
                m_graphics_queue->WaitIdle();
            } else if (m_synchronization) {
                m_synchronization->WaitIdle();
            }
        } catch (...) {
        }
    }

    releaseFrameCommandBuffers();
    m_current_command_buffer.reset();
    m_frame_render_graphs.clear();
    m_scene_renderer.shutdown();
    m_synchronization.reset();
    m_swapchain.reset();
    m_graphics_queue.reset();
    m_shader_compiler.reset();
    m_device.reset();
    m_surface.reset();
    m_adapter.reset();
    m_instance.reset();
    m_window = nullptr;
    m_native_window = nullptr;
    m_initialization_options = {};
    m_surface_format = luna::RHI::Format::UNDEFINED;
    m_frames_in_flight = 0;
    m_frame_index = 0;
    m_image_index = 0;
    m_swapchain_images_presented.clear();
    m_resize_requested = false;
    m_frame_started = false;
    m_initialized = false;
}

bool Renderer::isInitialized() const
{
    return m_initialized && m_device && m_swapchain && m_graphics_queue && m_synchronization;
}

bool Renderer::isRenderingEnabled() const
{
    const auto extent = getFramebufferExtent();
    return isInitialized() && extent.width > 0 && extent.height > 0;
}

bool Renderer::isImGuiEnabled() const
{
    return m_imgui_enabled;
}

void Renderer::requestResize()
{
    m_resize_requested = true;
}

bool Renderer::isResizeRequested() const
{
    return m_resize_requested;
}

void Renderer::setImGuiEnabled(bool enabled)
{
    m_imgui_enabled = enabled;
}

void Renderer::startFrame()
{
    if (!isRenderingEnabled()) {
        return;
    }

    handlePendingResize();
    if (!isRenderingEnabled()) {
        return;
    }

    try {
        m_synchronization->WaitForFrame(m_frame_index);

        if (m_frame_index < m_frame_command_buffers.size() && m_frame_command_buffers[m_frame_index]) {
            m_frame_command_buffers[m_frame_index]->ReturnToPool();
            m_frame_command_buffers[m_frame_index].reset();
        }
        if (m_frame_index < m_frame_render_graphs.size()) {
            m_frame_render_graphs[m_frame_index].reset();
        }

        int acquired_image_index = -1;
        const auto acquire_result =
            m_swapchain->AcquireNextImage(m_synchronization, static_cast<int>(m_frame_index), acquired_image_index);
        if (acquire_result != luna::RHI::Result::Success || acquired_image_index < 0) {
            throw std::runtime_error("Failed to acquire a swapchain image");
        }

        m_image_index = static_cast<uint32_t>(acquired_image_index);
        m_synchronization->ResetFrameFence(m_frame_index);

        m_current_command_buffer = m_device->CreateCommandBufferEncoder();
        m_current_command_buffer->Begin();
        if (m_frame_index >= m_frame_command_buffers.size()) {
            m_frame_command_buffers.resize(m_frames_in_flight);
        }
        m_frame_command_buffers[m_frame_index] = m_current_command_buffer;
        m_frame_started = true;
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("StartFrame failed, swapchain will be recreated: {}", error.what());
        m_frame_started = false;
        m_current_command_buffer.reset();
        m_resize_requested = true;
    }
}

void Renderer::renderFrame()
{
    if (!m_frame_started || !m_current_command_buffer || !m_swapchain) {
        return;
    }

    const auto extent = m_swapchain->GetExtent();
    const auto back_buffer = m_swapchain->GetBackBuffer(m_image_index);
    const bool was_presented =
        m_image_index < m_swapchain_images_presented.size() ? m_swapchain_images_presented[m_image_index] : false;
    const auto backend_type = m_instance ? m_instance->GetType() : m_initialization_options.backend;
    luna::rhi::RenderGraphBuilder graph_builder(luna::rhi::RenderGraphBuilder::FrameContext{
        .device = m_device,
        .command_buffer = m_current_command_buffer,
        .framebuffer_width = extent.width,
        .framebuffer_height = extent.height,
    });

    const auto back_buffer_handle =
        graph_builder.ImportTexture("SwapchainBackBuffer",
                                    back_buffer,
                                    was_presented ? luna::RHI::ResourceState::Present
                                                  : luna::RHI::ResourceState::Undefined);

    if (usesSceneRenderer(backend_type)) {
        const auto depth_handle = graph_builder.CreateTexture(luna::rhi::RenderGraphTextureDesc{
            .Name = "SceneDepthTexture",
            .Width = extent.width,
            .Height = extent.height,
            .Format = luna::RHI::Format::D32_FLOAT,
            .Usage = luna::RHI::TextureUsageFlags::DepthStencilAttachment,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        });

        m_scene_renderer.buildRenderGraph(graph_builder,
                                          SceneRenderer::RenderContext{
                                              .device = m_device,
                                              .compiler = m_shader_compiler,
                                              .backend_type = backend_type,
                                              .color_target = back_buffer_handle,
                                              .depth_target = depth_handle,
                                              .color_format = m_surface_format,
                                              .clear_color = m_clear_color,
                                              .framebuffer_width = extent.width,
                                              .framebuffer_height = extent.height,
                                          });
    } else {
        graph_builder.AddRasterPass(
            "ClearScene",
            [back_buffer_handle, clear_color = m_clear_color](luna::rhi::RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(back_buffer_handle,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(
                                            clear_color.r, clear_color.g, clear_color.b, clear_color.a));
            },
            [](luna::rhi::RenderGraphRasterPassContext& pass_context) {
                pass_context.beginRendering();
                pass_context.endRendering();
            });
    }

    if (m_imgui_enabled && backend_type == luna::RHI::BackendType::Vulkan) {
        graph_builder.AddRasterPass(
            "ImGui",
            [back_buffer_handle](luna::rhi::RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(
                    back_buffer_handle, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
            },
            [](luna::rhi::RenderGraphRasterPassContext& pass_context) {
                pass_context.beginRendering();
                luna::rhi::ImGuiVulkanContext::RenderDrawData(pass_context.commandBuffer());
                pass_context.endRendering();
            });
    }

    graph_builder.ExportTexture(back_buffer_handle, luna::RHI::ResourceState::Present);

    if (m_frame_index >= m_frame_render_graphs.size()) {
        m_frame_render_graphs.resize(m_frames_in_flight);
    }
    m_frame_render_graphs[m_frame_index] = graph_builder.Build();
    if (m_frame_render_graphs[m_frame_index]) {
        m_frame_render_graphs[m_frame_index]->execute();
    }

    if (m_image_index < m_swapchain_images_presented.size()) {
        m_swapchain_images_presented[m_image_index] = true;
    }
}

void Renderer::endFrame()
{
    if (!m_frame_started || !m_current_command_buffer) {
        return;
    }

    try {
        m_current_command_buffer->End();
        m_graphics_queue->Submit(m_current_command_buffer, m_synchronization, m_frame_index);
        const auto present_result = m_swapchain->Present(m_graphics_queue, m_synchronization, m_frame_index);
        if (present_result == luna::RHI::Result::OutOfDate || present_result == luna::RHI::Result::Suboptimal) {
            m_resize_requested = true;
        }
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("EndFrame failed, swapchain will be recreated: {}", error.what());
        m_resize_requested = true;
    }

    m_scene_renderer.clearSubmittedMeshes();
    m_current_command_buffer.reset();
    m_frame_started = false;
    if (m_frames_in_flight > 0) {
        m_frame_index = (m_frame_index + 1) % m_frames_in_flight;
    }
}

GLFWwindow* Renderer::getNativeWindow() const
{
    return m_native_window;
}

const luna::RHI::Ref<luna::RHI::Instance>& Renderer::getInstance() const
{
    return m_instance;
}

const luna::RHI::Ref<luna::RHI::Adapter>& Renderer::getAdapter() const
{
    return m_adapter;
}

const luna::RHI::Ref<luna::RHI::Device>& Renderer::getDevice() const
{
    return m_device;
}

const luna::RHI::Ref<luna::RHI::Queue>& Renderer::getGraphicsQueue() const
{
    return m_graphics_queue;
}

const luna::RHI::Ref<luna::RHI::Swapchain>& Renderer::getSwapchain() const
{
    return m_swapchain;
}

const luna::RHI::Ref<luna::RHI::Synchronization>& Renderer::getSynchronization() const
{
    return m_synchronization;
}

uint32_t Renderer::getFramesInFlight() const
{
    return m_frames_in_flight;
}

Camera& Renderer::getMainCamera()
{
    return m_main_camera;
}

const Camera& Renderer::getMainCamera() const
{
    return m_main_camera;
}

SceneRenderer& Renderer::getSceneRenderer()
{
    return m_scene_renderer;
}

const SceneRenderer& Renderer::getSceneRenderer() const
{
    return m_scene_renderer;
}

glm::vec4& Renderer::getClearColor()
{
    return m_clear_color;
}

const glm::vec4& Renderer::getClearColor() const
{
    return m_clear_color;
}

void Renderer::createSwapchain(uint32_t width, uint32_t height)
{
    if (!m_device || !m_surface || !m_adapter) {
        return;
    }

    const auto capabilities = m_surface->GetCapabilities(m_adapter);
    const auto formats = m_surface->GetSupportedFormats(m_adapter);
    const auto supported_present_modes = m_surface->GetSupportedPresentModes(m_adapter);
    const auto surface_format = chooseSurfaceFormat(formats);
    const auto requested_present_mode = m_initialization_options.present_mode;
    const auto selected_present_mode = choosePresentMode(supported_present_modes, requested_present_mode);

    const luna::RHI::Extent2D clamped_extent{
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };

    uint32_t min_image_count = (std::max) (2u, capabilities.minImageCount);
    if (capabilities.maxImageCount != 0) {
        min_image_count = (std::min) (min_image_count, capabilities.maxImageCount);
    }

    if (selected_present_mode != requested_present_mode) {
        LUNA_RENDERER_WARN("Requested present mode '{}' is unsupported; falling back to '{}'. Supported modes: {}",
                           presentModeToString(requested_present_mode),
                           presentModeToString(selected_present_mode),
                           describePresentModes(supported_present_modes));
    } else {
        LUNA_RENDERER_INFO("Using present mode '{}' (supported: {})",
                           presentModeToString(selected_present_mode),
                           describePresentModes(supported_present_modes));
    }

    m_swapchain = m_device->CreateSwapchain(luna::RHI::SwapchainBuilder()
                                                .SetExtent(clamped_extent)
                                                .SetFormat(surface_format.format)
                                                .SetColorSpace(surface_format.colorSpace)
                                                .SetPresentMode(selected_present_mode)
                                                .SetMinImageCount(min_image_count)
                                                .SetPreTransform(capabilities.currentTransform)
                                                .SetUsage(luna::RHI::SwapchainUsageFlags::ColorAttachment)
                                                .SetSurface(m_surface)
                                                .Build());

    m_surface_format = surface_format.format;
    m_frames_in_flight = (std::max) (1u, m_swapchain->GetImageCount() > 1 ? m_swapchain->GetImageCount() - 1 : 1u);
    m_synchronization = m_device->CreateSynchronization(m_frames_in_flight);
    m_frame_command_buffers.assign(m_frames_in_flight, {});
    m_frame_render_graphs.clear();
    m_frame_render_graphs.resize(m_frames_in_flight);
    m_swapchain_images_presented.assign(m_swapchain->GetImageCount(), false);
    if (m_frames_in_flight > 0) {
        m_frame_index %= m_frames_in_flight;
    } else {
        m_frame_index = 0;
    }

    if (m_imgui_enabled && m_instance && m_instance->GetType() == luna::RHI::BackendType::Vulkan) {
        luna::rhi::ImGuiVulkanContext::NotifySwapchainChanged(m_swapchain->GetImageCount());
    }
}

luna::RHI::Extent2D Renderer::getFramebufferExtent() const
{
    if (m_native_window == nullptr) {
        return {0, 0};
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_native_window, &width, &height);
    return {static_cast<uint32_t>((std::max) (width, 0)), static_cast<uint32_t>((std::max) (height, 0))};
}

void Renderer::handlePendingResize()
{
    if (!m_resize_requested || !m_device || !m_graphics_queue) {
        return;
    }

    const auto extent = getFramebufferExtent();
    if (extent.width == 0 || extent.height == 0) {
        return;
    }

    try {
        m_graphics_queue->WaitIdle();
        releaseFrameCommandBuffers();
        m_current_command_buffer.reset();
        m_frame_render_graphs.clear();
        m_swapchain.reset();
        m_synchronization.reset();
        createSwapchain(extent.width, extent.height);
        m_resize_requested = false;
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("Swapchain recreation failed: {}", error.what());
    }
}

void Renderer::releaseFrameCommandBuffers()
{
    for (auto& command_buffer : m_frame_command_buffers) {
        if (command_buffer) {
            command_buffer->ReturnToPool();
            command_buffer.reset();
        }
    }
    m_frame_command_buffers.clear();
}

} // namespace luna
