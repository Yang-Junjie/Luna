#include "Core/Log.h"
#include "Core/Window.h"
#include "Imgui/ImGuiContext.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererInternal.h"

#include <Buffer.h>
#include <Builders.h>
#include <Device.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstring>

#include <algorithm>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/trigonometric.hpp>
#include <Queue.h>
#include <stdexcept>
#include <Swapchain.h>
#include <Synchronization.h>

namespace luna {

namespace {

constexpr uint64_t kScenePickReadbackBufferSize = 256;

} // namespace

Renderer::Renderer() = default;

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(Window& window, InitializationOptions options)
{
    shutdown();

    m_window_context.window = &window;
    m_window_context.native_window = static_cast<GLFWwindow*>(window.getNativeWindow());
    m_runtime.initialization_options = std::move(options);
    LUNA_RENDERER_INFO("Initializing renderer with requested backend '{}' and present mode '{}'",
                       renderer_detail::backendTypeToString(m_runtime.initialization_options.backend),
                       renderer_detail::presentModeToString(m_runtime.initialization_options.present_mode));
    if (m_window_context.native_window == nullptr) {
        LUNA_RENDERER_ERROR("Cannot initialize renderer without a GLFW window");
        return false;
    }

    luna::RHI::NativeWindowHandle window_handle;
    window_handle.hWnd = glfwGetWin32Window(m_window_context.native_window);
    window_handle.hInst = GetModuleHandleW(nullptr);
    if (!window_handle.IsValid()) {
        LUNA_RENDERER_WARN("Native Win32 window handle is incomplete; surface creation may fail");
    }

    auto& device_context = m_device_context;
    auto& runtime = m_runtime;

    try {
        luna::RHI::InstanceCreateInfo instance_info;
        instance_info.type = runtime.initialization_options.backend;
        instance_info.applicationName = "Luna";
        instance_info.enabledFeatures.push_back(luna::RHI::InstanceFeature::Surface);
#ifndef NDEBUG
        instance_info.enabledFeatures.push_back(luna::RHI::InstanceFeature::ValidationLayer);
#endif

        device_context.instance = luna::RHI::Instance::Create(instance_info);
        LUNA_RENDERER_DEBUG("Created RHI instance for backend '{}'",
                            renderer_detail::backendTypeToString(device_context.instance->GetType()));
        device_context.shader_compiler = device_context.instance->CreateShaderCompiler();
        device_context.surface = device_context.instance->CreateSurface(window_handle);
        if (!device_context.surface) {
            throw std::runtime_error(
                "Failed to create surface for backend '" +
                std::string(renderer_detail::backendTypeToString(device_context.instance->GetType())) + "'");
        }

        const auto adapters = device_context.instance->EnumerateAdapters();
        LUNA_RENDERER_INFO("Enumerated {} renderer adapter(s)", adapters.size());
        for (size_t adapter_index = 0; adapter_index < adapters.size(); ++adapter_index) {
            if (!adapters[adapter_index]) {
                LUNA_RENDERER_DEBUG("Adapter {} is null", adapter_index);
                continue;
            }

            const auto properties = adapters[adapter_index]->GetProperties();
            LUNA_RENDERER_DEBUG("Adapter {}: '{}' type={} vendor=0x{:04x} device=0x{:04x} dedicated_vram={} MiB",
                                adapter_index,
                                properties.name,
                                renderer_detail::adapterTypeToString(properties.type),
                                properties.vendorID,
                                properties.deviceID,
                                properties.dedicatedVideoMemory / (1'024ull * 1'024ull));
        }
        device_context.adapter = renderer_detail::selectAdapter(adapters);
        if (!device_context.adapter) {
            throw std::runtime_error(
                "No compatible adapter available for backend '" +
                std::string(renderer_detail::backendTypeToString(device_context.instance->GetType())) + "'");
        }
        const auto selected_adapter_properties = device_context.adapter->GetProperties();
        LUNA_RENDERER_INFO("Selected renderer adapter '{}' ({}, {} MiB dedicated VRAM)",
                           selected_adapter_properties.name,
                           renderer_detail::adapterTypeToString(selected_adapter_properties.type),
                           selected_adapter_properties.dedicatedVideoMemory / (1'024ull * 1'024ull));

        luna::RHI::DeviceCreateInfo device_info;
        device_info.QueueRequests = {{luna::RHI::QueueType::Graphics, 1, 1.0f}};
        device_info.CompatibleSurface = device_context.surface;
        const bool sampler_anisotropy_supported =
            device_context.adapter->IsFeatureSupported(luna::RHI::DeviceFeature::SamplerAnisotropy);
        const bool independent_blending_supported =
            device_context.adapter->IsFeatureSupported(luna::RHI::DeviceFeature::IndependentBlending);
        if (sampler_anisotropy_supported) {
            device_info.EnabledFeatures.push_back(luna::RHI::DeviceFeature::SamplerAnisotropy);
        }
        if (independent_blending_supported) {
            device_info.EnabledFeatures.push_back(luna::RHI::DeviceFeature::IndependentBlending);
        }
        LUNA_RENDERER_DEBUG("Device feature support: sampler_anisotropy={} independent_blending={}",
                            sampler_anisotropy_supported,
                            independent_blending_supported);

        device_context.device = device_context.adapter->CreateDevice(device_info);
        device_context.graphics_queue = device_context.device->GetQueue(luna::RHI::QueueType::Graphics, 0);

        const auto extent = getFramebufferExtent();
        createSwapchain(extent.width, extent.height);
        LUNA_RENDERER_INFO("Initialized renderer backend '{}' with {} frame(s) in flight",
                           renderer_detail::backendTypeToString(device_context.instance->GetType()),
                           m_frame_resources.frames_in_flight);
    } catch (const std::exception& error) {
        LUNA_RENDERER_ERROR("Failed to initialize Renderer: {}", error.what());
        shutdown();
        return false;
    }

    runtime.main_camera.setPerspective(glm::radians(50.0f), 0.05f, 200.0f);
    runtime.main_camera.setPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    runtime.initialized = device_context.instance && device_context.adapter && device_context.device &&
                          device_context.surface && device_context.swapchain && device_context.graphics_queue &&
                          device_context.synchronization;
    runtime.resize_requested = false;
    runtime.frame_started = false;
    return runtime.initialized;
}

void Renderer::shutdown()
{
    const bool had_renderer_state = m_runtime.initialized || m_device_context.device || m_device_context.swapchain ||
                                    m_device_context.instance || !m_frame_resources.command_buffers.empty() ||
                                    m_scene_output.color || m_scene_output.depth || m_scene_output.pick;
    if (had_renderer_state) {
        LUNA_RENDERER_INFO("Shutting down renderer");
    }

    waitForGpuIdle();

    releaseFrameCommandBuffers();
    m_frame_resources.current_command_buffer.reset();
    m_frame_resources.render_graphs.clear();
    m_frame_resources.transient_texture_caches.clear();
    releaseSceneOutputTargets();
    m_scene_renderer.shutdown();
    m_device_context.synchronization.reset();
    m_device_context.swapchain.reset();
    m_device_context.graphics_queue.reset();
    m_device_context.shader_compiler.reset();
    m_device_context.device.reset();
    m_device_context.surface.reset();
    m_device_context.adapter.reset();
    m_device_context.instance.reset();
    m_device_context.surface_format = luna::RHI::Format::UNDEFINED;
    m_window_context = {};
    m_scene_output = {};
    m_frame_resources = {};
    m_runtime = {};

    if (had_renderer_state) {
        LUNA_RENDERER_INFO("Renderer shutdown complete");
    }
}

void Renderer::createSwapchain(uint32_t width, uint32_t height)
{
    auto& device_context = m_device_context;
    auto& frame_resources = m_frame_resources;
    auto& runtime = m_runtime;

    if (!device_context.device || !device_context.surface || !device_context.adapter) {
        LUNA_RENDERER_WARN("Cannot create swapchain because renderer device, surface, or adapter is missing");
        return;
    }

    const auto capabilities = device_context.surface->GetCapabilities(device_context.adapter);
    const auto formats = device_context.surface->GetSupportedFormats(device_context.adapter);
    const auto supported_present_modes = device_context.surface->GetSupportedPresentModes(device_context.adapter);
    const auto surface_format = renderer_detail::chooseSurfaceFormat(formats);
    const auto requested_present_mode = runtime.initialization_options.present_mode;
    const auto selected_present_mode =
        renderer_detail::choosePresentMode(supported_present_modes, requested_present_mode);

    const luna::RHI::Extent2D clamped_extent{
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
    LUNA_RENDERER_INFO("Creating swapchain: requested={}x{} clamped={}x{} surface_format={} ({})",
                       width,
                       height,
                       clamped_extent.width,
                       clamped_extent.height,
                       renderer_detail::formatToString(surface_format.format),
                       static_cast<int>(surface_format.format));

    uint32_t min_image_count = (std::max) (2u, capabilities.minImageCount);
    if (capabilities.maxImageCount != 0) {
        min_image_count = (std::min) (min_image_count, capabilities.maxImageCount);
    }

    if (selected_present_mode != requested_present_mode) {
        LUNA_RENDERER_WARN("Requested present mode '{}' is unsupported; falling back to '{}'. Supported modes: {}",
                           renderer_detail::presentModeToString(requested_present_mode),
                           renderer_detail::presentModeToString(selected_present_mode),
                           renderer_detail::describePresentModes(supported_present_modes));
    } else {
        LUNA_RENDERER_INFO("Using present mode '{}' (supported: {})",
                           renderer_detail::presentModeToString(selected_present_mode),
                           renderer_detail::describePresentModes(supported_present_modes));
    }

    device_context.swapchain =
        device_context.device->CreateSwapchain(luna::RHI::SwapchainBuilder()
                                                   .SetExtent(clamped_extent)
                                                   .SetFormat(surface_format.format)
                                                   .SetColorSpace(surface_format.colorSpace)
                                                   .SetPresentMode(selected_present_mode)
                                                   .SetMinImageCount(min_image_count)
                                                   .SetPreTransform(capabilities.currentTransform)
                                                   .SetUsage(luna::RHI::SwapchainUsageFlags::ColorAttachment)
                                                   .SetSurface(device_context.surface)
                                                   .Build());
    if (!device_context.swapchain) {
        throw std::runtime_error("Failed to create swapchain");
    }

    device_context.surface_format = surface_format.format;
    frame_resources.frames_in_flight =
        (std::max) (1u,
                    device_context.swapchain->GetImageCount() > 1 ? device_context.swapchain->GetImageCount() - 1 : 1u);
    device_context.synchronization = device_context.device->CreateSynchronization(frame_resources.frames_in_flight);
    if (!device_context.synchronization) {
        throw std::runtime_error("Failed to create frame synchronization objects");
    }
    frame_resources.command_buffers.assign(frame_resources.frames_in_flight, {});
    frame_resources.render_graphs.clear();
    frame_resources.render_graphs.resize(frame_resources.frames_in_flight);
    frame_resources.transient_texture_caches.clear();
    frame_resources.transient_texture_caches.resize(frame_resources.frames_in_flight);
    ensureScenePickReadbackBuffers();
    frame_resources.swapchain_images_presented.assign(device_context.swapchain->GetImageCount(), false);
    if (frame_resources.frames_in_flight > 0) {
        frame_resources.frame_index %= frame_resources.frames_in_flight;
    } else {
        frame_resources.frame_index = 0;
    }

    if (runtime.imgui_enabled) {
        luna::rhi::ImGuiRhiContext::NotifyFrameResourcesChanged(frame_resources.frames_in_flight);
    }

    LUNA_RENDERER_INFO("Created swapchain {}x{} with {} image(s), {} frame(s) in flight",
                       clamped_extent.width,
                       clamped_extent.height,
                       device_context.swapchain->GetImageCount(),
                       frame_resources.frames_in_flight);
}

luna::RHI::Extent2D Renderer::getFramebufferExtent() const
{
    if (m_window_context.native_window == nullptr) {
        return {0, 0};
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window_context.native_window, &width, &height);
    return {static_cast<uint32_t>((std::max) (width, 0)), static_cast<uint32_t>((std::max) (height, 0))};
}

void Renderer::handlePendingResize()
{
    auto& device_context = m_device_context;
    auto& frame_resources = m_frame_resources;
    auto& runtime = m_runtime;

    if (!runtime.resize_requested || !device_context.device || !device_context.graphics_queue) {
        return;
    }

    const auto extent = getFramebufferExtent();
    if (extent.width == 0 || extent.height == 0) {
        LUNA_RENDERER_DEBUG("Resize requested while framebuffer is minimized; delaying swapchain recreation");
        return;
    }

    try {
        LUNA_RENDERER_INFO("Recreating swapchain for framebuffer resize to {}x{}", extent.width, extent.height);
        device_context.graphics_queue->WaitIdle();
        releaseFrameCommandBuffers();
        frame_resources.current_command_buffer.reset();
        frame_resources.render_graphs.clear();
        frame_resources.transient_texture_caches.clear();
        device_context.swapchain.reset();
        device_context.synchronization.reset();
        createSwapchain(extent.width, extent.height);
        runtime.resize_requested = false;
        LUNA_RENDERER_INFO("Swapchain recreation complete");
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("Swapchain recreation failed: {}", error.what());
    }
}

void Renderer::releaseFrameCommandBuffers()
{
    size_t released_count = 0;
    for (auto& command_buffer : m_frame_resources.command_buffers) {
        if (command_buffer) {
            command_buffer->ReturnToPool();
            command_buffer.reset();
            ++released_count;
        }
    }
    if (released_count > 0 || !m_frame_resources.scene_pick_readback_slots.empty() ||
        !m_frame_resources.transient_texture_caches.empty()) {
        LUNA_RENDERER_DEBUG(
            "Released {} frame command buffer(s), {} transient cache(s), {} scene-pick readback slot(s)",
            released_count,
            m_frame_resources.transient_texture_caches.size(),
            m_frame_resources.scene_pick_readback_slots.size());
    }
    m_frame_resources.command_buffers.clear();
    m_frame_resources.transient_texture_caches.clear();
    m_frame_resources.scene_pick_readback_slots.clear();
}

void Renderer::ensureScenePickReadbackBuffers()
{
    auto& frame_resources = m_frame_resources;
    if (!m_device_context.device || frame_resources.frames_in_flight == 0) {
        if (!frame_resources.scene_pick_readback_slots.empty()) {
            LUNA_RENDERER_DEBUG("Clearing scene-pick readback buffers because frame resources are unavailable");
        }
        frame_resources.scene_pick_readback_slots.clear();
        return;
    }

    frame_resources.scene_pick_readback_slots.clear();
    frame_resources.scene_pick_readback_slots.resize(frame_resources.frames_in_flight);
    LUNA_RENDERER_DEBUG("Creating {} scene-pick readback buffer(s)", frame_resources.frames_in_flight);
    for (uint32_t frame_index = 0; frame_index < frame_resources.frames_in_flight; ++frame_index) {
        frame_resources.scene_pick_readback_slots[frame_index].buffer =
            m_device_context.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                      .SetSize(kScenePickReadbackBufferSize)
                                                      .SetUsage(luna::RHI::BufferUsageFlags::TransferDst)
                                                      .SetMemoryUsage(luna::RHI::BufferMemoryUsage::GpuToCpu)
                                                      .SetName("ScenePickReadback_" + std::to_string(frame_index))
                                                      .Build());
        frame_resources.scene_pick_readback_slots[frame_index].pending = false;
        if (!frame_resources.scene_pick_readback_slots[frame_index].buffer) {
            LUNA_RENDERER_WARN("Failed to create scene-pick readback buffer for frame {}", frame_index);
        }
    }
}

void Renderer::collectCompletedScenePickResult(uint32_t frame_index)
{
    if (frame_index >= m_frame_resources.scene_pick_readback_slots.size()) {
        return;
    }

    auto& slot = m_frame_resources.scene_pick_readback_slots[frame_index];
    if (!slot.pending || !slot.buffer) {
        return;
    }

    uint32_t picked_id = 0;
    if (void* mapped = slot.buffer->Map()) {
        std::memcpy(&picked_id, mapped, sizeof(picked_id));
        slot.buffer->Unmap();
    } else {
        LUNA_RENDERER_WARN("Failed to map scene-pick readback buffer for frame {}", frame_index);
    }

    slot.pending = false;
    m_scene_output.completed_pick_id = picked_id;
    LUNA_RENDERER_DEBUG("Completed scene pick readback on frame {} with entity id {}", frame_index, picked_id);
}

void Renderer::waitForGpuIdle() noexcept
{
    if (!m_device_context.device) {
        return;
    }

    try {
        if (m_device_context.graphics_queue) {
            LUNA_RENDERER_TRACE("Waiting for graphics queue idle");
            m_device_context.graphics_queue->WaitIdle();
        } else if (m_device_context.synchronization) {
            LUNA_RENDERER_TRACE("Waiting for renderer synchronization idle");
            m_device_context.synchronization->WaitIdle();
        }
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("Ignoring GPU idle wait failure during renderer teardown: {}", error.what());
    } catch (...) {
        LUNA_RENDERER_WARN("Ignoring unknown GPU idle wait failure during renderer teardown");
    }
}

} // namespace luna
