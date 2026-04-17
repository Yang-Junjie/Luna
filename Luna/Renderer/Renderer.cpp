#include "Core/Log.h"
#include "Core/Window.h"
#include "Imgui/ImGuiContext.h"
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

Cacao::Ref<Cacao::Adapter> selectAdapter(const std::vector<Cacao::Ref<Cacao::Adapter>>& adapters)
{
    if (adapters.empty()) {
        return {};
    }

    const auto discrete_adapter =
        std::find_if(adapters.begin(), adapters.end(), [](const Cacao::Ref<Cacao::Adapter>& adapter) {
            return adapter && adapter->GetAdapterType() == Cacao::AdapterType::Discrete;
        });
    return discrete_adapter != adapters.end() ? *discrete_adapter : adapters.front();
}

Cacao::SurfaceFormat chooseSurfaceFormat(const std::vector<Cacao::SurfaceFormat>& formats)
{
    const auto preferred = std::find_if(formats.begin(), formats.end(), [](const Cacao::SurfaceFormat& format) {
        return format.format == Cacao::Format::BGRA8_UNORM && format.colorSpace == Cacao::ColorSpace::SRGB_NONLINEAR;
    });
    if (preferred != formats.end()) {
        return *preferred;
    }

    const auto fallback = std::find_if(formats.begin(), formats.end(), [](const Cacao::SurfaceFormat& format) {
        return format.format == Cacao::Format::RGBA8_UNORM || format.format == Cacao::Format::BGRA8_UNORM;
    });
    if (fallback != formats.end()) {
        return *fallback;
    }

    return formats.empty() ? Cacao::SurfaceFormat{Cacao::Format::BGRA8_UNORM, Cacao::ColorSpace::SRGB_NONLINEAR}
                           : formats.front();
}

const char* presentModeToString(Cacao::PresentMode mode)
{
    switch (mode) {
        case Cacao::PresentMode::Immediate:
            return "Immediate";
        case Cacao::PresentMode::Mailbox:
            return "Mailbox";
        case Cacao::PresentMode::Fifo:
            return "Fifo";
        case Cacao::PresentMode::FifoRelaxed:
            return "FifoRelaxed";
        default:
            return "Unknown";
    }
}

bool isPresentModeSupported(const std::vector<Cacao::PresentMode>& supported_modes, Cacao::PresentMode mode)
{
    return std::find(supported_modes.begin(), supported_modes.end(), mode) != supported_modes.end();
}

std::string describePresentModes(const std::vector<Cacao::PresentMode>& supported_modes)
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

Cacao::PresentMode choosePresentMode(const std::vector<Cacao::PresentMode>& supported_modes,
                                     Cacao::PresentMode requested_mode)
{
    if (isPresentModeSupported(supported_modes, requested_mode)) {
        return requested_mode;
    }

    switch (requested_mode) {
        case Cacao::PresentMode::Mailbox:
            for (const auto fallback_mode :
                 std::array{Cacao::PresentMode::Immediate, Cacao::PresentMode::FifoRelaxed, Cacao::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case Cacao::PresentMode::Immediate:
            for (const auto fallback_mode :
                 std::array{Cacao::PresentMode::Mailbox, Cacao::PresentMode::FifoRelaxed, Cacao::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case Cacao::PresentMode::FifoRelaxed:
            for (const auto fallback_mode :
                 std::array{Cacao::PresentMode::Fifo, Cacao::PresentMode::Immediate, Cacao::PresentMode::Mailbox}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case Cacao::PresentMode::Fifo:
            if (isPresentModeSupported(supported_modes, Cacao::PresentMode::FifoRelaxed)) {
                return Cacao::PresentMode::FifoRelaxed;
            }
            break;
        default:
            break;
    }

    return supported_modes.empty() ? Cacao::PresentMode::Fifo : supported_modes.front();
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
        LUNA_CORE_ERROR("Cannot initialize Vulkan renderer without a GLFW window");
        return false;
    }

    Cacao::NativeWindowHandle window_handle;
    window_handle.hWnd = glfwGetWin32Window(m_native_window);
    window_handle.hInst = GetModuleHandleW(nullptr);

    try {
        Cacao::InstanceCreateInfo instance_info;
        instance_info.type = Cacao::BackendType::Vulkan;
        instance_info.applicationName = "Luna";
        instance_info.enabledFeatures.push_back(Cacao::InstanceFeature::Surface);
#ifndef NDEBUG
        instance_info.enabledFeatures.push_back(Cacao::InstanceFeature::ValidationLayer);
#endif

        m_instance = Cacao::Instance::Create(instance_info);
        m_surface = m_instance->CreateSurface(window_handle);

        const auto adapters = m_instance->EnumerateAdapters();
        m_adapter = selectAdapter(adapters);
        if (!m_adapter) {
            throw std::runtime_error("No Vulkan adapter available");
        }

        Cacao::DeviceCreateInfo device_info;
        device_info.QueueRequests = {{Cacao::QueueType::Graphics, 1, 1.0f}};
        device_info.CompatibleSurface = m_surface;
        if (m_adapter->IsFeatureSupported(Cacao::DeviceFeature::SamplerAnisotropy)) {
            device_info.EnabledFeatures.push_back(Cacao::DeviceFeature::SamplerAnisotropy);
        }

        m_device = m_adapter->CreateDevice(device_info);
        m_graphics_queue = m_device->GetQueue(Cacao::QueueType::Graphics, 0);

        const auto extent = getFramebufferExtent();
        createSwapchain(extent.width, extent.height);
    } catch (const std::exception& error) {
        LUNA_CORE_ERROR("Failed to initialize Renderer: {}", error.what());
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
    m_scene_renderer.shutdown();
    m_synchronization.reset();
    m_swapchain.reset();
    m_graphics_queue.reset();
    m_device.reset();
    m_surface.reset();
    m_adapter.reset();
    m_instance.reset();
    m_window = nullptr;
    m_native_window = nullptr;
    m_initialization_options = {};
    m_surface_format = Cacao::Format::UNDEFINED;
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

        int acquired_image_index = -1;
        const auto acquire_result =
            m_swapchain->AcquireNextImage(m_synchronization, static_cast<int>(m_frame_index), acquired_image_index);
        if (acquire_result != Cacao::Result::Success || acquired_image_index < 0) {
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
        LUNA_CORE_WARN("StartFrame failed, swapchain will be recreated: {}", error.what());
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
    m_current_command_buffer->TransitionImage(back_buffer,
                                              was_presented ? Cacao::ImageTransition::PresentToColorAttachment
                                                            : Cacao::ImageTransition::UndefinedToColorAttachment);

    m_scene_renderer.render(SceneRenderer::RenderContext{
        .device = m_device,
        .command_buffer = m_current_command_buffer,
        .color_target = back_buffer,
        .color_format = m_surface_format,
        .clear_color = m_clear_color,
        .framebuffer_width = extent.width,
        .framebuffer_height = extent.height,
    });

    if (m_imgui_enabled) {
        luna::rhi::ImGuiVulkanContext::RenderFrame(*m_current_command_buffer, back_buffer, extent.width, extent.height);
    }

    m_current_command_buffer->TransitionImage(back_buffer, Cacao::ImageTransition::ColorAttachmentToPresent);
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
        if (present_result == Cacao::Result::OutOfDate || present_result == Cacao::Result::Suboptimal) {
            m_resize_requested = true;
        }
    } catch (const std::exception& error) {
        LUNA_CORE_WARN("EndFrame failed, swapchain will be recreated: {}", error.what());
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

const Cacao::Ref<Cacao::Instance>& Renderer::getInstance() const
{
    return m_instance;
}

const Cacao::Ref<Cacao::Adapter>& Renderer::getAdapter() const
{
    return m_adapter;
}

const Cacao::Ref<Cacao::Device>& Renderer::getDevice() const
{
    return m_device;
}

const Cacao::Ref<Cacao::Queue>& Renderer::getGraphicsQueue() const
{
    return m_graphics_queue;
}

const Cacao::Ref<Cacao::Swapchain>& Renderer::getSwapchain() const
{
    return m_swapchain;
}

const Cacao::Ref<Cacao::Synchronization>& Renderer::getSynchronization() const
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

    const Cacao::Extent2D clamped_extent{
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };

    uint32_t min_image_count = (std::max) (2u, capabilities.minImageCount);
    if (capabilities.maxImageCount != 0) {
        min_image_count = (std::min) (min_image_count, capabilities.maxImageCount);
    }

    if (selected_present_mode != requested_present_mode) {
        LUNA_CORE_WARN("Requested present mode '{}' is unsupported; falling back to '{}'. Supported modes: {}",
                       presentModeToString(requested_present_mode),
                       presentModeToString(selected_present_mode),
                       describePresentModes(supported_present_modes));
    } else {
        LUNA_CORE_INFO("Using present mode '{}' (supported: {})",
                       presentModeToString(selected_present_mode),
                       describePresentModes(supported_present_modes));
    }

    m_swapchain = m_device->CreateSwapchain(Cacao::SwapchainBuilder()
                                                .SetExtent(clamped_extent)
                                                .SetFormat(surface_format.format)
                                                .SetColorSpace(surface_format.colorSpace)
                                                .SetPresentMode(selected_present_mode)
                                                .SetMinImageCount(min_image_count)
                                                .SetPreTransform(capabilities.currentTransform)
                                                .SetUsage(Cacao::SwapchainUsageFlags::ColorAttachment)
                                                .SetSurface(m_surface)
                                                .Build());

    m_surface_format = surface_format.format;
    m_frames_in_flight = (std::max) (1u, m_swapchain->GetImageCount() > 1 ? m_swapchain->GetImageCount() - 1 : 1u);
    m_synchronization = m_device->CreateSynchronization(m_frames_in_flight);
    m_frame_command_buffers.assign(m_frames_in_flight, {});
    m_swapchain_images_presented.assign(m_swapchain->GetImageCount(), false);
    if (m_frames_in_flight > 0) {
        m_frame_index %= m_frames_in_flight;
    } else {
        m_frame_index = 0;
    }

    if (m_imgui_enabled) {
        luna::rhi::ImGuiVulkanContext::NotifySwapchainChanged(m_swapchain->GetImageCount());
    }
}

Cacao::Extent2D Renderer::getFramebufferExtent() const
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
        m_swapchain.reset();
        m_synchronization.reset();
        createSwapchain(extent.width, extent.height);
        m_resize_requested = false;
    } catch (const std::exception& error) {
        LUNA_CORE_WARN("Swapchain recreation failed: {}", error.what());
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
