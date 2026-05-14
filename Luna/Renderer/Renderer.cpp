#include "Core/Log.h"
#include "Core/Window.h"
#include "Imgui/ImGuiContext.h"
#include "Platform/Common/NativeWindowHandle.h"
#include "Renderer/RenderFlow/DefaultRenderFlow.h"
#include "Renderer/RenderFlow/RenderFlow.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererUtilities.h"

#include <Buffer.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <Device.h>
#include <QueryPool.h>
#include <Queue.h>
#include <Swapchain.h>
#include <Synchronization.h>

#include <cstring>

#include <algorithm>
#include <array>
#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
#include <stdexcept>

namespace luna {

namespace {

constexpr uint64_t kScenePickReadbackBufferSize = 256;
constexpr uint32_t kRenderGraphGpuTimestampQueryCount = 512;

const luna::RHI::Ref<luna::RHI::Texture>& emptyRendererTextureRef()
{
    static const luna::RHI::Ref<luna::RHI::Texture> empty_ref{};
    return empty_ref;
}

const char* sceneOutputModeToString(Renderer::SceneOutputMode mode)
{
    switch (mode) {
        case Renderer::SceneOutputMode::Swapchain:
            return "Swapchain";
        case Renderer::SceneOutputMode::OffscreenTexture:
            return "OffscreenTexture";
        default:
            return "Unknown";
    }
}

const char* renderDebugViewModeToString(RenderDebugViewMode mode)
{
    switch (mode) {
        case RenderDebugViewMode::None:
            return "None";
        case RenderDebugViewMode::Velocity:
            return "Velocity";
        case RenderDebugViewMode::HistoryValidity:
            return "HistoryValidity";
        case RenderDebugViewMode::ShadowCascades:
            return "ShadowCascades";
        case RenderDebugViewMode::BaseColor:
            return "BaseColor";
        case RenderDebugViewMode::Normal:
            return "Normal";
        case RenderDebugViewMode::Metallic:
            return "Metallic";
        case RenderDebugViewMode::Roughness:
            return "Roughness";
        case RenderDebugViewMode::DirectLighting:
            return "DirectLighting";
        case RenderDebugViewMode::SpecularIbl:
            return "SpecularIBL";
        case RenderDebugViewMode::BloomInput:
            return "BloomInput";
        case RenderDebugViewMode::BloomPrefilter:
            return "BloomPrefilter";
        case RenderDebugViewMode::BloomMip0:
            return "BloomMip0";
        case RenderDebugViewMode::BloomMip1:
            return "BloomMip1";
        case RenderDebugViewMode::BloomMip2:
            return "BloomMip2";
        case RenderDebugViewMode::BloomMip3:
            return "BloomMip3";
        case RenderDebugViewMode::BloomMip4:
            return "BloomMip4";
        case RenderDebugViewMode::BloomMip5:
            return "BloomMip5";
        case RenderDebugViewMode::BloomComposite:
            return "BloomComposite";
        default:
            return "Unknown";
    }
}

const char* swapchainResultToString(luna::RHI::Result result)
{
    switch (result) {
        case luna::RHI::Result::Success:
            return "Success";
        case luna::RHI::Result::Timeout:
            return "Timeout";
        case luna::RHI::Result::NotReady:
            return "NotReady";
        case luna::RHI::Result::Suboptimal:
            return "Suboptimal";
        case luna::RHI::Result::OutOfDate:
            return "OutOfDate";
        case luna::RHI::Result::DeviceLost:
            return "DeviceLost";
        case luna::RHI::Result::Error:
        default:
            return "Error";
    }
}

} // namespace

Renderer::Renderer()
{
    m_default_scene_viewport_id = createSceneViewport();
}

Renderer::~Renderer()
{
    shutdown();
}

Renderer::SceneViewportId Renderer::createSceneViewport()
{
    const SceneViewportId id = m_next_scene_viewport_id++;
    auto state = std::make_unique<SceneViewportState>();
    m_scene_viewports.push_back(SceneViewportSlot{
        .id = id,
        .state = std::move(state),
    });
    return id;
}

void Renderer::destroySceneViewport(SceneViewportId id)
{
    if (id == kInvalidSceneViewportId) {
        return;
    }

    const auto slot_it = std::find_if(m_scene_viewports.begin(),
                                      m_scene_viewports.end(),
                                      [id](const SceneViewportSlot& slot) {
                                          return slot.id == id;
                                      });
    if (slot_it == m_scene_viewports.end()) {
        return;
    }

    if (slot_it->state) {
        releaseSceneOutputTargets(*slot_it->state);
    }
    m_scene_viewports.erase(slot_it);
    if (m_default_scene_viewport_id == id) {
        m_default_scene_viewport_id = kInvalidSceneViewportId;
    }
}

Renderer::SceneViewportState* Renderer::findSceneViewport(SceneViewportId id)
{
    const auto slot_it = std::find_if(m_scene_viewports.begin(),
                                      m_scene_viewports.end(),
                                      [id](const SceneViewportSlot& slot) {
                                          return slot.id == id;
                                      });
    if (slot_it == m_scene_viewports.end()) {
        return nullptr;
    }
    return slot_it->state.get();
}

const Renderer::SceneViewportState* Renderer::findSceneViewport(SceneViewportId id) const
{
    const auto slot_it = std::find_if(m_scene_viewports.begin(),
                                      m_scene_viewports.end(),
                                      [id](const SceneViewportSlot& slot) {
                                          return slot.id == id;
                                      });
    if (slot_it == m_scene_viewports.end()) {
        return nullptr;
    }
    return slot_it->state.get();
}

Renderer::SceneViewportState* Renderer::findSceneViewportByHandle(SceneViewportHandle handle)
{
    return findSceneViewport(handle);
}

const Renderer::SceneViewportState* Renderer::findSceneViewportByHandle(SceneViewportHandle handle) const
{
    return findSceneViewport(handle);
}

Renderer::SceneViewportState& Renderer::defaultSceneViewport()
{
    if (SceneViewportState* viewport = findSceneViewport(m_default_scene_viewport_id)) {
        return *viewport;
    }

    m_default_scene_viewport_id = createSceneViewport();
    return *findSceneViewport(m_default_scene_viewport_id);
}

const Renderer::SceneViewportState& Renderer::defaultSceneViewport() const
{
    const SceneViewportState* viewport = findSceneViewport(m_default_scene_viewport_id);
    if (!viewport) {
        throw std::logic_error("Renderer default scene viewport is missing");
    }
    return *viewport;
}

bool Renderer::init(Window& window, InitializationOptions options)
{
    shutdown();
    SceneViewportState& default_viewport = defaultSceneViewport();

    m_window_context.window = &window;
    m_window_context.native_window = static_cast<GLFWwindow*>(window.getNativeWindow());
    m_runtime.initialization_options = std::move(options);
    LUNA_RENDERER_INFO("Initializing renderer with requested backend '{}' and present mode '{}'",
                       luna::RHI::BackendTypeToString(m_runtime.initialization_options.backend),
                       renderer_detail::presentModeToString(m_runtime.initialization_options.present_mode));
    if (m_window_context.native_window == nullptr) {
        LUNA_RENDERER_ERROR("Cannot initialize renderer without a GLFW window");
        return false;
    }

    const luna::RHI::NativeWindowHandle window_handle = createNativeWindowHandle(window);
    if (!window_handle.IsValid()) {
        LUNA_RENDERER_WARN("Native {} window handle is incomplete; surface creation may fail",
                           nativeWindowPlatformName());
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
        device_context.capabilities = luna::RHI::makeCapabilitiesForBackend(device_context.instance->GetType());
        LUNA_RENDERER_DEBUG("Created RHI instance for backend '{}'",
                            luna::RHI::BackendTypeToString(device_context.instance->GetType()));
        LUNA_RENDERER_DEBUG("RHI capabilities: default_flow={} imgui={} scene_pick_readback={} gpu_timestamp={} "
                            "gpu_timestamp_disjoint={} "
                            "projection_y_flip={} imgui_uv_y_flip={} pick_y_matches_display={}",
                            device_context.capabilities.supports_default_render_flow,
                            device_context.capabilities.supports_imgui,
                            device_context.capabilities.supports_scene_pick_readback,
                            device_context.capabilities.supports_gpu_timestamp,
                            device_context.capabilities.gpu_timestamp_uses_disjoint_query,
                            device_context.capabilities.conventions.requires_projection_y_flip,
                            device_context.capabilities.conventions.imgui_render_target_requires_uv_y_flip,
                            device_context.capabilities.conventions.scene_pick_y_matches_display_y);
        device_context.shader_compiler = device_context.instance->CreateShaderCompiler();
        device_context.surface = device_context.instance->CreateSurface(window_handle);
        if (!device_context.surface) {
            throw std::runtime_error(
                "Failed to create surface for backend '" +
                std::string(luna::RHI::BackendTypeToString(device_context.instance->GetType())) + "'");
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
                std::string(luna::RHI::BackendTypeToString(device_context.instance->GetType())) + "'");
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
                           luna::RHI::BackendTypeToString(device_context.instance->GetType()),
                           m_frame_resources.frames_in_flight);
        default_viewport.render_flow = std::make_unique<DefaultRenderFlow>();
    } catch (const std::exception& error) {
        LUNA_RENDERER_ERROR("Failed to initialize Renderer: {}", error.what());
        shutdown();
        return false;
    }

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
                                    std::any_of(m_scene_viewports.begin(),
                                                m_scene_viewports.end(),
                                                [](const SceneViewportSlot& slot) {
                                                    const auto* viewport = slot.state.get();
                                                    return viewport != nullptr &&
                                                           (viewport->output.color || viewport->output.depth ||
                                                            viewport->output.pick || viewport->output.debug_color);
                                                });
    if (had_renderer_state) {
        LUNA_RENDERER_INFO("Shutting down renderer");
    }

    waitForGpuIdle();

    releaseFrameCommandBuffers();
    for (auto& slot : m_scene_viewports) {
        if (!slot.state) {
            continue;
        }
        releaseSceneOutputTargets(*slot.state);
        slot.state->render_flow.reset();
        slot.state->world.clear();
        slot.state->feature_history = {};
        slot.state->view_history.reset();
        slot.state->output = {};
    }
    m_frame_resources.current_command_buffer.reset();
    m_frame_resources.render_graphs.clear();
    m_frame_resources.transient_texture_caches.clear();
    m_device_context.synchronization.reset();
    m_device_context.swapchain.reset();
    m_device_context.graphics_queue.reset();
    m_device_context.shader_compiler.reset();
    m_device_context.device.reset();
    m_device_context.surface.reset();
    m_device_context.adapter.reset();
    m_device_context.instance.reset();
    m_device_context.surface_format = luna::RHI::Format::UNDEFINED;
    m_device_context.capabilities = {};
    m_window_context = {};
    m_frame_resources = {};
    m_runtime = {};
    m_scene_viewports.clear();
    m_default_scene_viewport_id = kInvalidSceneViewportId;
    m_next_scene_viewport_id = 1;
    m_default_scene_viewport_id = createSceneViewport();
    m_last_render_graph_profile = {};

    if (had_renderer_state) {
        LUNA_RENDERER_INFO("Renderer shutdown complete");
    }
}

void Renderer::startFrame()
{
    if (!isRenderingEnabled()) {
        LUNA_RENDERER_FRAME_TRACE("Skipping StartFrame because rendering is disabled");
        return;
    }

    handlePendingResize();
    if (!isRenderingEnabled()) {
        LUNA_RENDERER_FRAME_TRACE("Skipping StartFrame after resize handling because rendering is disabled");
        return;
    }

    auto& device_context = m_device_context;
    auto& frame_resources = m_frame_resources;
    auto& runtime = m_runtime;

    try {
        device_context.synchronization->WaitForFrame(frame_resources.frame_index);
        collectCompletedGpuTiming(frame_resources.frame_index);
        if (SceneViewportState* viewport = findSceneViewport(m_default_scene_viewport_id)) {
            collectCompletedScenePickResult(*viewport, frame_resources.frame_index);
        }

        if (frame_resources.frame_index < frame_resources.command_buffers.size() &&
            frame_resources.command_buffers[frame_resources.frame_index]) {
            frame_resources.command_buffers[frame_resources.frame_index]->ReturnToPool();
            frame_resources.command_buffers[frame_resources.frame_index].reset();
        }
        if (frame_resources.frame_index < frame_resources.render_graphs.size()) {
            frame_resources.render_graphs[frame_resources.frame_index].reset();
        }
        if (frame_resources.frame_index < frame_resources.transient_texture_caches.size()) {
            frame_resources.transient_texture_caches[frame_resources.frame_index].BeginFrame();
        }

        int acquired_image_index = -1;
        const auto acquire_result = device_context.swapchain->AcquireNextImage(
            device_context.synchronization, static_cast<int>(frame_resources.frame_index), acquired_image_index);
        if (acquire_result != luna::RHI::Result::Success || acquired_image_index < 0) {
            LUNA_RENDERER_WARN("AcquireNextImage failed for frame {}: result={} image_index={}",
                               frame_resources.frame_index,
                               swapchainResultToString(acquire_result),
                               acquired_image_index);
            throw std::runtime_error("Failed to acquire a swapchain image");
        }

        frame_resources.image_index = static_cast<uint32_t>(acquired_image_index);
        device_context.synchronization->ResetFrameFence(frame_resources.frame_index);
        LUNA_RENDERER_FRAME_DEBUG(
            "Started frame {} using swapchain image {}", frame_resources.frame_index, frame_resources.image_index);

        frame_resources.current_command_buffer = device_context.device->CreateCommandBufferEncoder();
        frame_resources.current_command_buffer->Begin();
        if (frame_resources.frame_index >= frame_resources.command_buffers.size()) {
            frame_resources.command_buffers.resize(frame_resources.frames_in_flight);
        }
        frame_resources.command_buffers[frame_resources.frame_index] = frame_resources.current_command_buffer;
        runtime.frame_started = true;
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("StartFrame failed, swapchain will be recreated: {}", error.what());
        runtime.frame_started = false;
        frame_resources.current_command_buffer.reset();
        runtime.resize_requested = true;
        invalidateRenderFeatureHistory(defaultSceneViewport(), render_flow::RenderFeatureHistoryInvalidationFlags::Resize);
    }
}

void Renderer::renderFrame()
{
    auto& device_context = m_device_context;
    auto& frame_resources = m_frame_resources;
    auto& runtime = m_runtime;
    if (!runtime.frame_started || !frame_resources.current_command_buffer || !device_context.swapchain) {
        LUNA_RENDERER_FRAME_TRACE(
            "Skipping RenderFrame because the frame has not started or swapchain state is incomplete");
        return;
    }

    struct RenderedSceneViewport {
        SceneViewportState* viewport{nullptr};
        SceneViewportRenderResult result{};
    };

    std::vector<RenderedSceneViewport> rendered_viewports;
    rendered_viewports.reserve(m_scene_viewports.size());

    auto& viewport = defaultSceneViewport();
    auto& scene_output = viewport.output;
    viewport.feature_history.has_pending_frame = false;

    const auto extent = device_context.swapchain->GetExtent();
    const auto back_buffer = device_context.swapchain->GetBackBuffer(frame_resources.image_index);
    const bool was_presented = frame_resources.image_index < frame_resources.swapchain_images_presented.size()
                                   ? frame_resources.swapchain_images_presented[frame_resources.image_index]
                                   : false;
    const auto backend_type =
        device_context.instance ? device_context.instance->GetType() : runtime.initialization_options.backend;
    const auto& capabilities = device_context.capabilities;
    LUNA_RENDERER_FRAME_DEBUG(
        "Building frame {} render graph: swapchain_extent={}x{} backend={} scene_output_mode={} imgui={}",
        frame_resources.frame_index,
        extent.width,
        extent.height,
        luna::RHI::BackendTypeToString(backend_type),
        scene_output.mode == SceneOutputMode::OffscreenTexture ? "OffscreenTexture" : "Swapchain",
        runtime.imgui_enabled);

    luna::RenderGraphTransientTextureCache* transient_texture_cache =
        (frame_resources.frame_index < frame_resources.transient_texture_caches.size())
            ? &frame_resources.transient_texture_caches[frame_resources.frame_index]
            : nullptr;
    FrameResources::GpuTimingSlot* gpu_timing_slot =
        (frame_resources.frame_index < frame_resources.gpu_timing_slots.size())
            ? &frame_resources.gpu_timing_slots[frame_resources.frame_index]
            : nullptr;

    luna::RenderGraphBuilder graph_builder(
        luna::RenderGraphBuilder::FrameContext{
            .device = device_context.device,
            .command_buffer = frame_resources.current_command_buffer,
            .timestamp_query_pool = gpu_timing_slot != nullptr ? gpu_timing_slot->query_pool : nullptr,
            .timestamp_disjoint_query_pool =
                gpu_timing_slot != nullptr ? gpu_timing_slot->disjoint_query_pool : nullptr,
            .timestamp_query_capacity =
                gpu_timing_slot != nullptr && gpu_timing_slot->query_pool ? gpu_timing_slot->query_pool->GetCount() : 0,
            .frame_index = frame_resources.frame_index,
            .profiling_enabled = runtime.render_graph_profiling_enabled,
            .framebuffer_width = extent.width,
            .framebuffer_height = extent.height,
        },
        transient_texture_cache);

    const auto back_buffer_handle = graph_builder.ImportTexture("SwapchainBackBuffer",
                                                                back_buffer,
                                                                was_presented ? luna::RHI::ResourceState::Present
                                                                              : luna::RHI::ResourceState::Undefined);
    const bool pick_readback_slot_available =
        frame_resources.frame_index < frame_resources.scene_pick_readback_slots.size() &&
        frame_resources.scene_pick_readback_slots[frame_resources.frame_index].buffer;
    const SceneViewportRenderResult scene_result = renderSceneViewport(
        viewport,
        graph_builder,
        SceneViewportRenderRequest{
            .back_buffer = back_buffer_handle,
            .framebuffer_extent = extent,
            .surface_format = device_context.surface_format,
            .backend_type = backend_type,
            .clear_color = runtime.clear_color,
            .frame_index = frame_resources.frame_index,
            .pick_readback_supported = capabilities.supports_scene_pick_readback,
            .pick_readback_slot_available = pick_readback_slot_available,
        });
    rendered_viewports.push_back(RenderedSceneViewport{
        .viewport = &viewport,
        .result = scene_result,
    });

    for (auto& slot : m_scene_viewports) {
        if (slot.id == m_default_scene_viewport_id || !slot.state) {
            continue;
        }

        SceneViewportState& extra_viewport = *slot.state;
        extra_viewport.feature_history.has_pending_frame = false;
        auto& extra_output = extra_viewport.output;
        const bool extra_viewport_ready = extra_output.mode == SceneOutputMode::OffscreenTexture &&
                                          extra_output.extent.width > 0 && extra_output.extent.height > 0 &&
                                          extra_output.color && extra_output.depth && extra_output.pick;
        if (!extra_viewport_ready) {
            continue;
        }

        SceneViewportRenderResult extra_result = renderSceneViewport(
            extra_viewport,
            graph_builder,
            SceneViewportRenderRequest{
                .back_buffer = {},
                .framebuffer_extent = extra_output.extent,
                .surface_format = device_context.surface_format,
                .backend_type = backend_type,
                .clear_color = runtime.clear_color,
                .frame_index = frame_resources.frame_index,
                .pick_readback_supported = false,
                .pick_readback_slot_available = false,
            });
        rendered_viewports.push_back(RenderedSceneViewport{
            .viewport = &extra_viewport,
            .result = extra_result,
        });
    }

    if (runtime.imgui_enabled) {
        graph_builder.AddRasterPass(
            "ImGui",
             [back_buffer_handle,
             render_scene_to_swapchain = scene_result.render_to_swapchain,
             scene_output_valid = scene_result.output_valid,
             rendered_viewports,
             clear_color = runtime.clear_color](luna::RenderGraphRasterPassBuilder& pass_builder) {
                for (const RenderedSceneViewport& rendered_viewport : rendered_viewports) {
                    if (!rendered_viewport.result.render_to_offscreen || !rendered_viewport.result.output_valid) {
                        continue;
                    }

                    pass_builder.ReadTexture(rendered_viewport.result.color);
                    if (rendered_viewport.result.debug.isValid()) {
                        pass_builder.ReadTexture(rendered_viewport.result.debug);
                    }
                }

                pass_builder.WriteColor(
                    back_buffer_handle,
                    render_scene_to_swapchain && scene_output_valid ? luna::RHI::AttachmentLoadOp::Load
                                                                    : luna::RHI::AttachmentLoadOp::Clear,
                    luna::RHI::AttachmentStoreOp::Store,
                    luna::RHI::ClearValue::ColorFloat(clear_color.r, clear_color.g, clear_color.b, clear_color.a));
            },
            [this, back_buffer_handle](luna::RenderGraphRasterPassContext& pass_context) {
                pass_context.beginRendering();
                luna::ImGuiRhiContext::RenderDrawData(pass_context.commandBuffer(),
                                                      pass_context.getTexture(back_buffer_handle),
                                                      pass_context.framebufferWidth(),
                                                      pass_context.framebufferHeight(),
                                                      m_frame_resources.frame_index);
                pass_context.endRendering();
            });
    } else if (!scene_result.render_to_swapchain) {
        graph_builder.AddRasterPass(
            "PresentClear",
            [back_buffer_handle, clear_color = runtime.clear_color](luna::RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(
                    back_buffer_handle,
                    luna::RHI::AttachmentLoadOp::Clear,
                    luna::RHI::AttachmentStoreOp::Store,
                    luna::RHI::ClearValue::ColorFloat(clear_color.r, clear_color.g, clear_color.b, clear_color.a));
            },
            [](luna::RenderGraphRasterPassContext& pass_context) {
                pass_context.beginRendering();
                pass_context.endRendering();
            });
    }

    graph_builder.ExportTexture(back_buffer_handle, luna::RHI::ResourceState::Present);

    if (frame_resources.frame_index >= frame_resources.render_graphs.size()) {
        frame_resources.render_graphs.resize(frame_resources.frames_in_flight);
    }
    frame_resources.render_graphs[frame_resources.frame_index] = graph_builder.Build();
    if (frame_resources.render_graphs[frame_resources.frame_index]) {
        LUNA_RENDERER_FRAME_DEBUG("Executing frame {} render graph with {} compiled pass(es)",
                                  frame_resources.frame_index,
                                  frame_resources.render_graphs[frame_resources.frame_index]->passes().size());
        frame_resources.render_graphs[frame_resources.frame_index]->execute();
        for (const RenderedSceneViewport& rendered_viewport : rendered_viewports) {
            if (rendered_viewport.viewport == nullptr || !rendered_viewport.result.output_valid ||
                !rendered_viewport.viewport->render_flow) {
                continue;
            }

            if (rendered_viewport.viewport->render_flow->commitFrame()) {
                rendered_viewport.viewport->view_history.commitFrame();
                commitStagedRenderFeatureFrameContext(*rendered_viewport.viewport);
            }
        }
        const RenderGraphProfileSnapshot& profile = frame_resources.render_graphs[frame_resources.frame_index]->profile();
        if (runtime.render_graph_profiling_enabled) {
            if (!storePendingGpuTimingProfile(frame_resources.frame_index, profile)) {
                m_last_render_graph_profile = profile;
            } else if (m_last_render_graph_profile.Passes.empty()) {
                m_last_render_graph_profile = profile;
            }
        }
    } else {
        if (runtime.render_graph_profiling_enabled) {
            m_last_render_graph_profile = {};
        }
        LUNA_RENDERER_WARN("Render graph build returned null for frame {}", frame_resources.frame_index);
    }

    if (scene_result.render_to_offscreen && scene_result.output_valid && scene_output.queued_pick_request.has_value() &&
        !capabilities.supports_scene_pick_readback) {
        LUNA_RENDERER_WARN("Scene pick readback is not supported on backend '{}'; dropping pick request",
                           luna::RHI::BackendTypeToString(backend_type));
        scene_output.queued_pick_request.reset();
    }

    if (scene_result.issue_pick_readback) {
        auto& readback_slot = frame_resources.scene_pick_readback_slots[frame_resources.frame_index];
        const auto pick_request = *scene_output.queued_pick_request;
        LUNA_RENDERER_DEBUG("Queued scene pick readback at ({}, {}) on frame {}",
                            pick_request.x,
                            pick_request.y,
                            frame_resources.frame_index);
        const std::array<luna::RHI::BufferImageCopy, 1> copy_regions{luna::RHI::BufferImageCopy{
            .BufferOffset = 0,
            .BufferRowLength = 0,
            .BufferImageHeight = 0,
            .ImageSubresource =
                {
                    .AspectMask = luna::RHI::ImageAspectFlags::Color,
                    .MipLevel = 0,
                    .BaseArrayLayer = 0,
                    .LayerCount = 1,
                },
            .ImageOffsetX = static_cast<int32_t>(pick_request.x),
            .ImageOffsetY = static_cast<int32_t>(pick_request.y),
            .ImageOffsetZ = 0,
            .ImageExtentWidth = 1,
            .ImageExtentHeight = 1,
            .ImageExtentDepth = 1,
        }};

        frame_resources.current_command_buffer->CopyImageToBuffer(
            scene_output.pick, luna::RHI::ResourceState::CopySource, readback_slot.buffer, copy_regions);
        readback_slot.pending = true;
        scene_output.queued_pick_request.reset();
    }

    for (const RenderedSceneViewport& rendered_viewport : rendered_viewports) {
        if (rendered_viewport.viewport == nullptr || !rendered_viewport.result.render_to_offscreen ||
            !rendered_viewport.result.output_valid) {
            continue;
        }

        auto& output = rendered_viewport.viewport->output;
        output.color_state = luna::RHI::ResourceState::ShaderRead;
        output.depth_state =
            rendered_viewport.result.depth.isValid() ? luna::RHI::ResourceState::Common : luna::RHI::ResourceState::Undefined;
        output.pick_state =
            rendered_viewport.result.pick.isValid()
                ? (rendered_viewport.result.issue_pick_readback ? luna::RHI::ResourceState::CopySource
                                                                : luna::RHI::ResourceState::Common)
                : luna::RHI::ResourceState::Undefined;
        output.debug_color_state = rendered_viewport.result.debug.isValid()
                                       ? luna::RHI::ResourceState::ShaderRead
                                       : luna::RHI::ResourceState::Undefined;
    }

    if (frame_resources.image_index < frame_resources.swapchain_images_presented.size()) {
        frame_resources.swapchain_images_presented[frame_resources.image_index] = true;
    }
}

void Renderer::endFrame()
{
    auto& device_context = m_device_context;
    auto& frame_resources = m_frame_resources;
    auto& runtime = m_runtime;

    if (!runtime.frame_started || !frame_resources.current_command_buffer) {
        LUNA_RENDERER_FRAME_TRACE("Skipping EndFrame because no frame is active");
        return;
    }

    try {
        frame_resources.current_command_buffer->End();
        device_context.graphics_queue->Submit(
            frame_resources.current_command_buffer, device_context.synchronization, frame_resources.frame_index);
        const auto present_result = device_context.swapchain->Present(
            device_context.graphics_queue, device_context.synchronization, frame_resources.frame_index);
        if (present_result == luna::RHI::Result::OutOfDate || present_result == luna::RHI::Result::Suboptimal) {
            LUNA_RENDERER_WARN("Present returned {}; scheduling swapchain recreation",
                               swapchainResultToString(present_result));
            runtime.resize_requested = true;
            invalidateRenderFeatureHistory(defaultSceneViewport(),
                                           render_flow::RenderFeatureHistoryInvalidationFlags::Resize);
        } else if (present_result != luna::RHI::Result::Success) {
            LUNA_RENDERER_WARN("Present returned {}", swapchainResultToString(present_result));
        } else {
            LUNA_RENDERER_FRAME_DEBUG("Presented frame {} using swapchain image {}",
                                      frame_resources.frame_index,
                                      frame_resources.image_index);
        }
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("EndFrame failed, swapchain will be recreated: {}", error.what());
        runtime.resize_requested = true;
        invalidateRenderFeatureHistory(defaultSceneViewport(),
                                       render_flow::RenderFeatureHistoryInvalidationFlags::Resize);
    }

    frame_resources.current_command_buffer.reset();
    runtime.frame_started = false;
    if (frame_resources.frames_in_flight > 0) {
        frame_resources.frame_index = (frame_resources.frame_index + 1) % frame_resources.frames_in_flight;
    }
    LUNA_RENDERER_FRAME_TRACE("Advanced to frame index {}", frame_resources.frame_index);
}

bool Renderer::isInitialized() const
{
    return m_runtime.initialized && m_device_context.device && m_device_context.swapchain &&
           m_device_context.graphics_queue && m_device_context.synchronization;
}

bool Renderer::isRenderingEnabled() const
{
    const auto extent = getFramebufferExtent();
    return isInitialized() && extent.width > 0 && extent.height > 0;
}

bool Renderer::isImGuiEnabled() const
{
    return m_runtime.imgui_enabled;
}

void Renderer::requestResize()
{
    m_runtime.resize_requested = true;
    invalidateRenderFeatureHistory(defaultSceneViewport(), render_flow::RenderFeatureHistoryInvalidationFlags::Resize);
    LUNA_RENDERER_DEBUG("Renderer resize requested");
}

bool Renderer::isResizeRequested() const
{
    return m_runtime.resize_requested;
}

void Renderer::notifyCameraCut()
{
    invalidateRenderFeatureHistory(defaultSceneViewport(),
                                   render_flow::RenderFeatureHistoryInvalidationFlags::CameraCut);
    LUNA_RENDERER_DEBUG("Render feature history invalidated by camera cut");
}

void Renderer::setImGuiEnabled(bool enabled)
{
    if (m_runtime.imgui_enabled == enabled) {
        LUNA_RENDERER_TRACE("Ignoring redundant ImGui enabled state change: {}", enabled);
        return;
    }

    m_runtime.imgui_enabled = enabled;
    LUNA_RENDERER_INFO("Renderer ImGui rendering {}", enabled ? "enabled" : "disabled");
}

Renderer::SceneOutputMode Renderer::getSceneOutputMode() const
{
    return defaultSceneViewport().output.mode;
}

void Renderer::setSceneOutputMode(SceneOutputMode mode)
{
    setSceneViewportOutputMode(getDefaultSceneViewportHandle(), mode);
}

void Renderer::setSceneOutputSize(uint32_t width, uint32_t height)
{
    setSceneViewportOutputSize(getDefaultSceneViewportHandle(), width, height);
}

luna::RHI::Extent2D Renderer::getSceneOutputSize() const
{
    return getSceneViewportOutputSize(getDefaultSceneViewportHandle());
}

const luna::RHI::Ref<luna::RHI::Texture>& Renderer::getSceneOutputTexture() const
{
    return getSceneViewportOutputTexture(getDefaultSceneViewportHandle());
}

Renderer::SceneViewportHandle Renderer::getDefaultSceneViewportHandle() const noexcept
{
    return m_default_scene_viewport_id;
}

Renderer::SceneViewportHandle Renderer::createSceneViewportHandle()
{
    const SceneViewportHandle handle = createSceneViewport();
    SceneViewportState* viewport = findSceneViewportByHandle(handle);
    if (viewport != nullptr) {
        viewport->output.mode = SceneOutputMode::OffscreenTexture;
        if (m_runtime.initialized && !viewport->render_flow) {
            viewport->render_flow = std::make_unique<DefaultRenderFlow>();
        }
        invalidateRenderFeatureHistory(*viewport,
                                       render_flow::RenderFeatureHistoryInvalidationFlags::SceneOutputChanged);
    }
    return handle;
}

void Renderer::destroySceneViewportHandle(SceneViewportHandle handle)
{
    if (handle == kInvalidSceneViewportHandle || handle == m_default_scene_viewport_id) {
        return;
    }

    destroySceneViewport(handle);
}

bool Renderer::isSceneViewportHandleValid(SceneViewportHandle handle) const
{
    return findSceneViewportByHandle(handle) != nullptr;
}

void Renderer::setSceneViewportOutputMode(SceneViewportHandle handle, SceneOutputMode mode)
{
    SceneViewportState* viewport = findSceneViewportByHandle(handle);
    if (viewport == nullptr) {
        return;
    }

    auto& output = viewport->output;

    if (output.mode == mode) {
        return;
    }

    LUNA_RENDERER_INFO("Changing scene output mode from '{}' to '{}'",
                       sceneOutputModeToString(output.mode),
                       sceneOutputModeToString(mode));
    output.mode = mode;
    ++output.generation;
    invalidateRenderFeatureHistory(*viewport, render_flow::RenderFeatureHistoryInvalidationFlags::SceneOutputChanged);

    if (output.mode != SceneOutputMode::OffscreenTexture) {
        if (output.color || output.depth || output.pick) {
            LUNA_RENDERER_DEBUG("Waiting for GPU before releasing offscreen scene output targets");
            waitForGpuIdle();
        }
        releaseSceneOutputTargets(*viewport);
        return;
    }

    if (output.extent.width > 0 && output.extent.height > 0 &&
        !hasMatchingSceneOutputTargets(*viewport, output.extent.width, output.extent.height)) {
        ensureSceneOutputTargets(*viewport, output.extent.width, output.extent.height);
    }
}

void Renderer::setSceneViewportOutputSize(SceneViewportHandle handle, uint32_t width, uint32_t height)
{
    SceneViewportState* viewport = findSceneViewportByHandle(handle);
    if (viewport == nullptr) {
        return;
    }

    auto& output = viewport->output;
    const bool size_changed = output.extent.width != width || output.extent.height != height;
    output.extent = {width, height};
    if (size_changed) {
        LUNA_RENDERER_INFO("Scene output size set to {}x{}", width, height);
        ++output.generation;
        invalidateRenderFeatureHistory(*viewport,
                                       render_flow::RenderFeatureHistoryInvalidationFlags::Resize |
                                       render_flow::RenderFeatureHistoryInvalidationFlags::SceneOutputChanged);
    }

    if (!size_changed || output.mode != SceneOutputMode::OffscreenTexture || width == 0 || height == 0) {
        if (size_changed && (width == 0 || height == 0)) {
            LUNA_RENDERER_DEBUG("Scene output target creation delayed because requested size is zero");
        }
        return;
    }

    ensureSceneOutputTargets(*viewport, width, height);
}

luna::RHI::Extent2D Renderer::getSceneViewportOutputSize(SceneViewportHandle handle) const
{
    const SceneViewportState* viewport = findSceneViewportByHandle(handle);
    return viewport != nullptr ? viewport->output.extent : luna::RHI::Extent2D{};
}

const luna::RHI::Ref<luna::RHI::Texture>& Renderer::getSceneViewportOutputTexture(SceneViewportHandle handle) const
{
    const SceneViewportState* viewport = findSceneViewportByHandle(handle);
    return viewport != nullptr ? viewport->output.color : emptyRendererTextureRef();
}

RenderWorld& Renderer::getSceneViewportRenderWorld(SceneViewportHandle handle)
{
    if (SceneViewportState* viewport = findSceneViewportByHandle(handle)) {
        return viewport->world;
    }
    return defaultSceneViewport().world;
}

const RenderWorld& Renderer::getSceneViewportRenderWorld(SceneViewportHandle handle) const
{
    if (const SceneViewportState* viewport = findSceneViewportByHandle(handle)) {
        return viewport->world;
    }
    return defaultSceneViewport().world;
}

void Renderer::setRenderDebugViewMode(RenderDebugViewMode mode)
{
    auto& viewport = defaultSceneViewport();
    auto& output = viewport.output;

    if (output.debug_view_mode == mode) {
        return;
    }

    LUNA_RENDERER_INFO("Render debug view changed from '{}' to '{}'",
                       renderDebugViewModeToString(output.debug_view_mode),
                       renderDebugViewModeToString(mode));
    output.debug_view_mode = mode;
    if (mode != RenderDebugViewMode::None && output.mode == SceneOutputMode::OffscreenTexture &&
        output.extent.width > 0 && output.extent.height > 0 &&
        !hasMatchingSceneOutputTargets(viewport, output.extent.width, output.extent.height)) {
        ensureSceneOutputTargets(viewport, output.extent.width, output.extent.height);
    }
}

RenderDebugViewMode Renderer::getRenderDebugViewMode() const
{
    return defaultSceneViewport().output.debug_view_mode;
}

void Renderer::setRenderDebugVelocityScale(float scale)
{
    defaultSceneViewport().output.debug_velocity_scale = glm::clamp(scale, 1.0f, 500.0f);
}

float Renderer::getRenderDebugVelocityScale() const
{
    return defaultSceneViewport().output.debug_velocity_scale;
}

const luna::RHI::Ref<luna::RHI::Texture>& Renderer::getRenderDebugOutputTexture() const
{
    return defaultSceneViewport().output.debug_color;
}

void Renderer::setScenePickDebugVisualizationEnabled(bool enabled)
{
    auto& output = defaultSceneViewport().output;
    if (output.pick_debug_visualization_enabled != enabled) {
        LUNA_RENDERER_INFO("Scene pick debug visualization {}", enabled ? "enabled" : "disabled");
    }
    output.pick_debug_visualization_enabled = enabled;
    if (!enabled) {
        output.debug_pick_marker = {};
    }
}

bool Renderer::isScenePickDebugVisualizationEnabled() const
{
    return defaultSceneViewport().output.pick_debug_visualization_enabled;
}

void Renderer::requestScenePick(uint32_t x, uint32_t y)
{
    auto& output = defaultSceneViewport().output;
    if (output.mode != SceneOutputMode::OffscreenTexture || !output.pick || output.extent.width == 0 ||
        output.extent.height == 0) {
        LUNA_RENDERER_WARN(
            "Ignoring scene pick request at ({}, {}) because offscreen pick output is unavailable", x, y);
        return;
    }

    const uint32_t clamped_x = (std::min) (x, output.extent.width - 1);
    const uint32_t clamped_y = (std::min) (y, output.extent.height - 1);
    if (clamped_x != x || clamped_y != y) {
        LUNA_RENDERER_DEBUG("Clamped scene pick request from ({}, {}) to ({}, {}) within {}x{}",
                            x,
                            y,
                            clamped_x,
                            clamped_y,
                            output.extent.width,
                            output.extent.height);
    } else {
        LUNA_RENDERER_DEBUG("Queued scene pick request at ({}, {})", clamped_x, clamped_y);
    }
    output.debug_pick_marker = SceneOutputState::PickDebugMarker{
        .x = clamped_x,
        .y = clamped_y,
        .valid = output.pick_debug_visualization_enabled,
    };
    output.queued_pick_request = SceneOutputState::PickRequest{
        .x = clamped_x,
        .y = clamped_y,
    };
}

std::optional<uint32_t> Renderer::consumeScenePickResult()
{
    auto& output = defaultSceneViewport().output;
    if (!output.completed_pick_id.has_value()) {
        return std::nullopt;
    }

    const uint32_t picked_id = *output.completed_pick_id;
    output.completed_pick_id.reset();
    LUNA_RENDERER_DEBUG("Consumed scene pick result: entity id {}", picked_id);
    return picked_id;
}

GLFWwindow* Renderer::getNativeWindow() const
{
    return m_window_context.native_window;
}

const luna::RHI::Ref<luna::RHI::Instance>& Renderer::getInstance() const
{
    return m_device_context.instance;
}

const luna::RHI::Ref<luna::RHI::Adapter>& Renderer::getAdapter() const
{
    return m_device_context.adapter;
}

const luna::RHI::RHICapabilities& Renderer::getCapabilities() const noexcept
{
    return m_device_context.capabilities;
}

const luna::RHI::Ref<luna::RHI::Device>& Renderer::getDevice() const
{
    return m_device_context.device;
}

const luna::RHI::Ref<luna::RHI::Queue>& Renderer::getGraphicsQueue() const
{
    return m_device_context.graphics_queue;
}

const luna::RHI::Ref<luna::RHI::Swapchain>& Renderer::getSwapchain() const
{
    return m_device_context.swapchain;
}

const luna::RHI::Ref<luna::RHI::Synchronization>& Renderer::getSynchronization() const
{
    return m_device_context.synchronization;
}

uint32_t Renderer::getFramesInFlight() const
{
    return m_frame_resources.frames_in_flight;
}

RenderWorld& Renderer::getRenderWorld()
{
    return defaultSceneViewport().world;
}

const RenderWorld& Renderer::getRenderWorld() const
{
    return defaultSceneViewport().world;
}

const RenderGraphProfileSnapshot& Renderer::getLastRenderGraphProfile() const
{
    return m_last_render_graph_profile;
}

void Renderer::setRenderGraphProfilingEnabled(bool enabled)
{
    if (m_runtime.render_graph_profiling_enabled == enabled) {
        return;
    }

    m_runtime.render_graph_profiling_enabled = enabled;
    if (enabled) {
        ensureGpuTimingResources();
        LUNA_RENDERER_INFO("RenderGraph profiling enabled");
        return;
    }

    for (auto& slot : m_frame_resources.gpu_timing_slots) {
        slot.pending = false;
        slot.query_count = 0;
    }
    LUNA_RENDERER_INFO("RenderGraph profiling disabled");
}

bool Renderer::isRenderGraphProfilingEnabled() const
{
    return m_runtime.render_graph_profiling_enabled;
}

bool Renderer::addDefaultRenderFeature(std::unique_ptr<render_flow::IRenderFeature> feature)
{
    auto* default_render_flow = dynamic_cast<DefaultRenderFlow*>(defaultSceneViewport().render_flow.get());
    if (!default_render_flow) {
        LUNA_RENDERER_ERROR("Cannot add default render feature because the active render flow is not DefaultRenderFlow");
        return false;
    }

    return default_render_flow->addFeature(std::move(feature));
}

std::vector<render_flow::RenderFeatureInfo> Renderer::getDefaultRenderFeatureInfos() const
{
    auto* default_render_flow = dynamic_cast<DefaultRenderFlow*>(defaultSceneViewport().render_flow.get());
    if (!default_render_flow) {
        return {};
    }

    return default_render_flow->featureInfos();
}

bool Renderer::setDefaultRenderFeatureEnabled(std::string_view name, bool enabled)
{
    auto* default_render_flow = dynamic_cast<DefaultRenderFlow*>(defaultSceneViewport().render_flow.get());
    if (!default_render_flow) {
        LUNA_RENDERER_ERROR("Cannot toggle default render feature because the active render flow is not DefaultRenderFlow");
        return false;
    }

    return default_render_flow->setFeatureEnabled(name, enabled);
}

std::vector<render_flow::RenderFeatureParameterInfo> Renderer::getDefaultRenderFeatureParameters(std::string_view name) const
{
    auto* default_render_flow = dynamic_cast<DefaultRenderFlow*>(defaultSceneViewport().render_flow.get());
    if (!default_render_flow) {
        return {};
    }

    return default_render_flow->featureParameters(name);
}

bool Renderer::setDefaultRenderFeatureParameter(std::string_view feature_name,
                                                std::string_view parameter_name,
                                                const render_flow::RenderFeatureParameterValue& value)
{
    auto* default_render_flow = dynamic_cast<DefaultRenderFlow*>(defaultSceneViewport().render_flow.get());
    if (!default_render_flow) {
        LUNA_RENDERER_ERROR("Cannot set default render feature parameter because the active render flow is not DefaultRenderFlow");
        return false;
    }

    return default_render_flow->setFeatureParameter(feature_name, parameter_name, value);
}

bool Renderer::configureDefaultRenderFlow(const DefaultRenderFlowConfigureFunction& configure_function)
{
    auto* default_render_flow = dynamic_cast<DefaultRenderFlow*>(defaultSceneViewport().render_flow.get());
    if (!default_render_flow) {
        LUNA_RENDERER_ERROR("Cannot configure default render flow because the active render flow is not DefaultRenderFlow");
        return false;
    }

    return default_render_flow->configure(configure_function);
}

glm::vec4& Renderer::getClearColor()
{
    return m_runtime.clear_color;
}

const glm::vec4& Renderer::getClearColor() const
{
    return m_runtime.clear_color;
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
    if (runtime.render_graph_profiling_enabled) {
        ensureGpuTimingResources();
    } else {
        frame_resources.gpu_timing_slots.clear();
    }
    ensureScenePickReadbackBuffers();
    frame_resources.swapchain_images_presented.assign(device_context.swapchain->GetImageCount(), false);
    if (frame_resources.frames_in_flight > 0) {
        frame_resources.frame_index %= frame_resources.frames_in_flight;
    } else {
        frame_resources.frame_index = 0;
    }

    if (runtime.imgui_enabled) {
        luna::ImGuiRhiContext::NotifyFrameResourcesChanged(frame_resources.frames_in_flight);
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
        invalidateRenderFeatureHistory(defaultSceneViewport(), render_flow::RenderFeatureHistoryInvalidationFlags::Resize);
        LUNA_RENDERER_INFO("Swapchain recreation complete");
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("Swapchain recreation failed: {}", error.what());
    }
}

Renderer::SceneViewportRenderResult Renderer::renderSceneViewport(SceneViewportState& viewport,
                                                                  luna::RenderGraphBuilder& graph_builder,
                                                                  const SceneViewportRenderRequest& request)
{
    auto& output = viewport.output;
    const auto& capabilities = m_device_context.capabilities;
    const bool render_to_offscreen = output.mode == SceneOutputMode::OffscreenTexture && output.extent.width > 0 &&
                                     output.extent.height > 0 && output.color && output.depth && output.pick;
    const bool render_to_swapchain = output.mode == SceneOutputMode::Swapchain;

    SceneViewportRenderResult result{
        .render_to_offscreen = render_to_offscreen,
        .render_to_swapchain = render_to_swapchain,
    };

    luna::RHI::Format scene_color_format = request.surface_format;
    luna::RHI::Format scene_debug_format = request.surface_format;
    uint32_t scene_width = 0;
    uint32_t scene_height = 0;

    if (render_to_swapchain) {
        result.color = request.back_buffer;
        scene_color_format = request.surface_format;
        scene_width = request.framebuffer_extent.width;
        scene_height = request.framebuffer_extent.height;

        if (capabilities.supports_default_render_flow) {
            result.depth = graph_builder.CreateTexture(luna::RenderGraphTextureDesc{
                .Name = "SceneDepthTexture",
                .Width = request.framebuffer_extent.width,
                .Height = request.framebuffer_extent.height,
                .Format = luna::RHI::Format::D32_FLOAT,
                .Usage = luna::RHI::TextureUsageFlags::DepthStencilAttachment |
                         luna::RHI::TextureUsageFlags::Sampled,
                .InitialState = luna::RHI::ResourceState::Undefined,
                .SampleCount = luna::RHI::SampleCount::Count1,
            });
            result.pick = graph_builder.CreateTexture(luna::RenderGraphTextureDesc{
                .Name = "ScenePickTexture",
                .Width = request.framebuffer_extent.width,
                .Height = request.framebuffer_extent.height,
                .Format = luna::RHI::Format::R32_UINT,
                .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
                .InitialState = luna::RHI::ResourceState::Undefined,
                .SampleCount = luna::RHI::SampleCount::Count1,
            });
        }
    } else if (render_to_offscreen) {
        result.color = graph_builder.ImportTexture(
            "SceneOutputColor", output.color, output.color_state, luna::RHI::ResourceState::ShaderRead);
        result.depth = graph_builder.ImportTexture(
            "SceneOutputDepth", output.depth, output.depth_state, luna::RHI::ResourceState::Common);
        result.pick = graph_builder.ImportTexture(
            "SceneOutputPick", output.pick, output.pick_state, luna::RHI::ResourceState::Common);
        if (output.debug_view_mode != RenderDebugViewMode::None && output.debug_color) {
            result.debug = graph_builder.ImportTexture("SceneDebugOutput",
                                                       output.debug_color,
                                                       output.debug_color_state,
                                                       luna::RHI::ResourceState::ShaderRead);
            scene_debug_format = output.debug_color->GetFormat();
        }
        scene_color_format = output.color ? output.color->GetFormat() : request.surface_format;
        scene_width = output.color ? output.color->GetWidth() : 0;
        scene_height = output.color ? output.color->GetHeight() : 0;
    }

    result.output_valid = result.color.isValid() && scene_width > 0 && scene_height > 0;
    if (!result.output_valid) {
        LUNA_RENDERER_FRAME_DEBUG("Scene output is invalid for frame {}: color_handle={} size={}x{}",
                                  request.frame_index,
                                  result.color.isValid(),
                                  scene_width,
                                  scene_height);
    }

    result.issue_pick_readback = render_to_offscreen && result.output_valid && result.pick.isValid() &&
                                 request.pick_readback_supported && output.queued_pick_request.has_value() &&
                                 request.pick_readback_slot_available;

    if (result.output_valid) {
        if (capabilities.supports_default_render_flow && result.depth.isValid() && viewport.render_flow) {
            SceneRenderContext scene_context{
                .device = m_device_context.device,
                .compiler = m_device_context.shader_compiler,
                .backend_type = request.backend_type,
                .capabilities = capabilities,
                .color_target = result.color,
                .depth_target = result.depth,
                .pick_target = result.pick,
                .color_format = scene_color_format,
                .clear_color = request.clear_color,
                .show_pick_debug_visualization = output.pick_debug_visualization_enabled,
                .debug_pick_pixel_x = output.debug_pick_marker.x,
                .debug_pick_pixel_y = output.debug_pick_marker.y,
                .show_pick_debug_marker = output.pick_debug_visualization_enabled && output.debug_pick_marker.valid,
                .debug_target = result.debug,
                .debug_format = scene_debug_format,
                .debug_view_mode = output.debug_view_mode,
                .debug_velocity_scale = output.debug_velocity_scale,
                .framebuffer_width = scene_width,
                .framebuffer_height = scene_height,
            };
            const render_flow::RenderFeatureFrameContext feature_frame_context =
                makeRenderFeatureFrameContext(viewport,
                                              request.backend_type,
                                              output.mode,
                                              request.frame_index,
                                              scene_width,
                                              scene_height);
            render_flow::RenderFeatureFrameContext feature_frame_context_with_view = feature_frame_context;
            feature_frame_context_with_view.view =
                viewport.view_history.beginFrame(viewport.world.camera(),
                                                 capabilities,
                                                 request.frame_index,
                                                 scene_width,
                                                 scene_height,
                                                 feature_frame_context.historyInvalidated());
            if (feature_frame_context.historyInvalidated()) {
                LUNA_RENDERER_FRAME_DEBUG("Render feature history invalidated for frame {}: flags=0x{:x}",
                                          request.frame_index,
                                          static_cast<uint32_t>(feature_frame_context.history_invalidation_flags));
            }
            RenderFlowContext flow_context(graph_builder, viewport.world, scene_context, feature_frame_context_with_view);
            viewport.render_flow->render(flow_context);
            if (viewport.world.hasCamera()) {
                stageRenderFeatureFrameContext(viewport, request.backend_type, output.mode, scene_width, scene_height);
            }
        } else {
            graph_builder.AddRasterPass(
                "ClearScene",
                [scene_color_handle = result.color,
                 clear_color = request.clear_color](luna::RenderGraphRasterPassBuilder& pass_builder) {
                    pass_builder.WriteColor(
                        scene_color_handle,
                        luna::RHI::AttachmentLoadOp::Clear,
                        luna::RHI::AttachmentStoreOp::Store,
                        luna::RHI::ClearValue::ColorFloat(clear_color.r, clear_color.g, clear_color.b, clear_color.a));
                },
                [](luna::RenderGraphRasterPassContext& pass_context) {
                    pass_context.beginRendering();
                    pass_context.endRendering();
                });
        }
    }

    if (render_to_offscreen && result.output_valid) {
        graph_builder.ExportTexture(result.color, luna::RHI::ResourceState::ShaderRead);
        if (result.depth.isValid()) {
            graph_builder.ExportTexture(result.depth, luna::RHI::ResourceState::Common);
        }
        if (result.pick.isValid()) {
            graph_builder.ExportTexture(result.pick,
                                        result.issue_pick_readback ? luna::RHI::ResourceState::CopySource
                                                                   : luna::RHI::ResourceState::Common);
        }
        if (result.debug.isValid()) {
            graph_builder.ExportTexture(result.debug, luna::RHI::ResourceState::ShaderRead);
        }
    }

    return result;
}

void Renderer::invalidateRenderFeatureHistory(SceneViewportState& viewport,
                                              render_flow::RenderFeatureHistoryInvalidationFlags flags) noexcept
{
    viewport.feature_history.pending_flags |= flags;
}

render_flow::RenderFeatureFrameContext Renderer::makeRenderFeatureFrameContext(
    const SceneViewportState& viewport,
    luna::RHI::BackendType backend_type,
    SceneOutputMode scene_output_mode,
    uint64_t frame_index,
    uint32_t framebuffer_width,
    uint32_t framebuffer_height) const
{
    render_flow::RenderFeatureHistoryInvalidationFlags invalidation_flags =
        viewport.feature_history.pending_flags;

    if (!viewport.feature_history.has_previous_frame) {
        invalidation_flags |= render_flow::RenderFeatureHistoryInvalidationFlags::FirstFrame;
    } else {
        if (viewport.feature_history.device != m_device_context.device.get()) {
            invalidation_flags |= render_flow::RenderFeatureHistoryInvalidationFlags::DeviceChanged;
        }
        if (viewport.feature_history.backend_type != backend_type) {
            invalidation_flags |= render_flow::RenderFeatureHistoryInvalidationFlags::BackendChanged;
        }
        if (viewport.feature_history.framebuffer_width != framebuffer_width ||
            viewport.feature_history.framebuffer_height != framebuffer_height) {
            invalidation_flags |= render_flow::RenderFeatureHistoryInvalidationFlags::Resize;
        }
        if (viewport.feature_history.scene_output_mode != scene_output_mode ||
            viewport.feature_history.scene_output_generation != viewport.output.generation) {
            invalidation_flags |= render_flow::RenderFeatureHistoryInvalidationFlags::SceneOutputChanged;
        }
    }

    return render_flow::RenderFeatureFrameContext{
        .device = m_device_context.device,
        .backend_type = backend_type,
        .capabilities = m_device_context.capabilities,
        .frame_index = frame_index,
        .framebuffer_width = framebuffer_width,
        .framebuffer_height = framebuffer_height,
        .history_invalidation_flags = invalidation_flags,
    };
}

void Renderer::stageRenderFeatureFrameContext(SceneViewportState& viewport,
                                              luna::RHI::BackendType backend_type,
                                              SceneOutputMode scene_output_mode,
                                              uint32_t framebuffer_width,
                                              uint32_t framebuffer_height) noexcept
{
    viewport.feature_history.has_pending_frame = true;
    viewport.feature_history.pending_device = m_device_context.device.get();
    viewport.feature_history.pending_backend_type = backend_type;
    viewport.feature_history.pending_scene_output_mode = scene_output_mode;
    viewport.feature_history.pending_framebuffer_width = framebuffer_width;
    viewport.feature_history.pending_framebuffer_height = framebuffer_height;
    viewport.feature_history.pending_scene_output_generation = viewport.output.generation;
}

void Renderer::commitStagedRenderFeatureFrameContext(SceneViewportState& viewport) noexcept
{
    if (!viewport.feature_history.has_pending_frame) {
        return;
    }

    viewport.feature_history.has_previous_frame = true;
    viewport.feature_history.device = viewport.feature_history.pending_device;
    viewport.feature_history.backend_type = viewport.feature_history.pending_backend_type;
    viewport.feature_history.scene_output_mode = viewport.feature_history.pending_scene_output_mode;
    viewport.feature_history.framebuffer_width = viewport.feature_history.pending_framebuffer_width;
    viewport.feature_history.framebuffer_height = viewport.feature_history.pending_framebuffer_height;
    viewport.feature_history.scene_output_generation = viewport.feature_history.pending_scene_output_generation;
    viewport.feature_history.pending_flags = render_flow::RenderFeatureHistoryInvalidationFlags::None;
    viewport.feature_history.has_pending_frame = false;
}

bool Renderer::hasMatchingSceneOutputTargets(const SceneViewportState& viewport, uint32_t width, uint32_t height) const
{
    const auto& output = viewport.output;
    return output.color && output.depth && output.pick && output.debug_color &&
           output.color->GetWidth() == width && output.color->GetHeight() == height &&
           output.depth->GetWidth() == width && output.depth->GetHeight() == height &&
           output.pick->GetWidth() == width && output.pick->GetHeight() == height &&
           output.debug_color->GetWidth() == width && output.debug_color->GetHeight() == height &&
           output.color->GetFormat() == m_device_context.surface_format &&
           output.depth->GetFormat() == luna::RHI::Format::D32_FLOAT &&
           output.pick->GetFormat() == luna::RHI::Format::R32_UINT &&
           output.debug_color->GetFormat() == m_device_context.surface_format;
}

void Renderer::ensureSceneOutputTargets(SceneViewportState& viewport, uint32_t width, uint32_t height)
{
    auto& output = viewport.output;

    if (!m_device_context.device || width == 0 || height == 0) {
        LUNA_RENDERER_WARN("Cannot create scene output targets: device_available={} size={}x{}",
                           static_cast<bool>(m_device_context.device),
                           width,
                           height);
        return;
    }

    if (hasMatchingSceneOutputTargets(viewport, width, height)) {
        LUNA_RENDERER_TRACE("Scene output targets already match requested size {}x{}", width, height);
        return;
    }

    LUNA_RENDERER_INFO("Creating scene output targets {}x{} using color format {} ({})",
                       width,
                       height,
                       renderer_detail::formatToString(m_device_context.surface_format),
                       static_cast<int>(m_device_context.surface_format));
    waitForGpuIdle();
    releaseSceneOutputTargets(viewport);

    output.color = m_device_context.device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(width, height)
            .SetFormat(m_device_context.surface_format)
            .SetUsage(luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName("SceneOutputColor")
            .Build());

    output.depth =
        m_device_context.device->CreateTexture(luna::RHI::TextureBuilder()
                                                   .SetSize(width, height)
                                                   .SetFormat(luna::RHI::Format::D32_FLOAT)
                                                   .SetUsage(luna::RHI::TextureUsageFlags::DepthStencilAttachment |
                                                             luna::RHI::TextureUsageFlags::Sampled)
                                                   .SetInitialState(luna::RHI::ResourceState::Undefined)
                                                   .SetName("SceneOutputDepth")
                                                   .Build());

    output.pick = m_device_context.device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(width, height)
            .SetFormat(luna::RHI::Format::R32_UINT)
            .SetUsage(luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled |
                      luna::RHI::TextureUsageFlags::TransferSrc)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName("SceneOutputPick")
            .Build());

    output.debug_color = m_device_context.device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(width, height)
            .SetFormat(m_device_context.surface_format)
            .SetUsage(luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName("SceneDebugOutput")
            .Build());

    output.color_state = luna::RHI::ResourceState::Undefined;
    output.depth_state = luna::RHI::ResourceState::Undefined;
    output.pick_state = luna::RHI::ResourceState::Undefined;
    output.debug_color_state = luna::RHI::ResourceState::Undefined;

    if (!output.color || !output.depth || !output.pick || !output.debug_color) {
        LUNA_RENDERER_ERROR("Failed to create complete scene output target set: color={} depth={} pick={} debug={}",
                            static_cast<bool>(output.color),
                            static_cast<bool>(output.depth),
                            static_cast<bool>(output.pick),
                            static_cast<bool>(output.debug_color));
    } else {
        ++output.generation;
        invalidateRenderFeatureHistory(viewport, render_flow::RenderFeatureHistoryInvalidationFlags::SceneOutputChanged);
        LUNA_RENDERER_INFO("Created scene output targets {}x{}", width, height);
    }
}

void Renderer::releaseSceneOutputTargets(SceneViewportState& viewport)
{
    auto& output = viewport.output;
    const bool had_targets = output.color || output.depth || output.pick || output.debug_color;
    output.color.reset();
    output.depth.reset();
    output.pick.reset();
    output.debug_color.reset();
    output.color_state = luna::RHI::ResourceState::Undefined;
    output.depth_state = luna::RHI::ResourceState::Undefined;
    output.pick_state = luna::RHI::ResourceState::Undefined;
    output.debug_color_state = luna::RHI::ResourceState::Undefined;
    output.queued_pick_request.reset();
    output.completed_pick_id.reset();
    output.debug_pick_marker = {};
    if (had_targets) {
        ++output.generation;
        invalidateRenderFeatureHistory(viewport, render_flow::RenderFeatureHistoryInvalidationFlags::SceneOutputChanged);
        LUNA_RENDERER_INFO("Released scene output targets");
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
        !m_frame_resources.gpu_timing_slots.empty() || !m_frame_resources.transient_texture_caches.empty()) {
        LUNA_RENDERER_DEBUG(
            "Released {} frame command buffer(s), {} transient cache(s), {} scene-pick readback slot(s), {} GPU timing slot(s)",
            released_count,
            m_frame_resources.transient_texture_caches.size(),
            m_frame_resources.scene_pick_readback_slots.size(),
            m_frame_resources.gpu_timing_slots.size());
    }
    m_frame_resources.command_buffers.clear();
    m_frame_resources.transient_texture_caches.clear();
    m_frame_resources.scene_pick_readback_slots.clear();
    m_frame_resources.gpu_timing_slots.clear();
}

void Renderer::ensureGpuTimingResources()
{
    auto& frame_resources = m_frame_resources;
    if (!m_device_context.device || !m_device_context.graphics_queue || frame_resources.frames_in_flight == 0) {
        frame_resources.gpu_timing_slots.clear();
        return;
    }
    if (!m_device_context.capabilities.supports_gpu_timestamp) {
        frame_resources.gpu_timing_slots.clear();
        LUNA_RENDERER_INFO("RenderGraph GPU timing is unavailable on backend '{}'",
                           luna::RHI::BackendTypeToString(m_device_context.capabilities.backend_type));
        return;
    }

    const double timestamp_period_ns = m_device_context.graphics_queue->GetTimestampPeriodNs();
    const bool use_disjoint_timestamps =
        timestamp_period_ns <= 0.0 && m_device_context.capabilities.gpu_timestamp_uses_disjoint_query;
    if (timestamp_period_ns <= 0.0 && !use_disjoint_timestamps) {
        frame_resources.gpu_timing_slots.clear();
        LUNA_RENDERER_INFO("RenderGraph GPU timing is unavailable on this RHI backend");
        return;
    }

    frame_resources.gpu_timing_slots.clear();
    frame_resources.gpu_timing_slots.resize(frame_resources.frames_in_flight);
    LUNA_RENDERER_INFO("Creating RenderGraph GPU timing query pools: frames={}, queries_per_frame={}, mode={}, period_ns={:.6f}",
                       frame_resources.frames_in_flight,
                       kRenderGraphGpuTimestampQueryCount,
                       use_disjoint_timestamps ? "disjoint" : "fixed-period",
                       timestamp_period_ns);
    for (uint32_t frame_index = 0; frame_index < frame_resources.frames_in_flight; ++frame_index) {
        auto& slot = frame_resources.gpu_timing_slots[frame_index];
        slot.query_pool = m_device_context.device->CreateQueryPool(luna::RHI::QueryPoolCreateInfo{
            .Type = luna::RHI::QueryType::Timestamp,
            .Count = kRenderGraphGpuTimestampQueryCount,
        });
        if (use_disjoint_timestamps) {
            slot.disjoint_query_pool = m_device_context.device->CreateQueryPool(luna::RHI::QueryPoolCreateInfo{
                .Type = luna::RHI::QueryType::TimestampDisjoint,
                .Count = 1,
            });
        } else {
            slot.disjoint_query_pool.reset();
        }
        slot.profile = {};
        slot.query_count = 0;
        slot.pending = false;
        slot.uses_disjoint_timestamps = use_disjoint_timestamps;
        if (!slot.query_pool || (use_disjoint_timestamps && !slot.disjoint_query_pool)) {
            LUNA_RENDERER_WARN("Failed to create RenderGraph GPU timing query pool for frame {}", frame_index);
            slot.query_pool.reset();
            slot.disjoint_query_pool.reset();
            slot.uses_disjoint_timestamps = false;
        }
    }
}

void Renderer::collectCompletedGpuTiming(uint32_t frame_index)
{
    if (frame_index >= m_frame_resources.gpu_timing_slots.size()) {
        return;
    }

    auto& slot = m_frame_resources.gpu_timing_slots[frame_index];
    if (!slot.pending || !slot.query_pool || slot.query_count == 0) {
        return;
    }

    std::vector<uint64_t> timestamps;
    if (!slot.query_pool->GetResults(0, slot.query_count, timestamps, false)) {
        LUNA_RENDERER_FRAME_TRACE("RenderGraph GPU timing results are not ready for frame {}", frame_index);
        return;
    }

    double timestamp_period_ns =
        m_device_context.graphics_queue ? m_device_context.graphics_queue->GetTimestampPeriodNs() : 0.0;
    if (slot.uses_disjoint_timestamps) {
        if (!slot.disjoint_query_pool) {
            slot.pending = false;
            return;
        }

        luna::RHI::TimestampDisjointResult disjoint_result{};
        if (!slot.disjoint_query_pool->GetTimestampDisjointResult(0, disjoint_result, false)) {
            LUNA_RENDERER_FRAME_TRACE("RenderGraph GPU timing disjoint result is not ready for frame {}", frame_index);
            return;
        }

        if (disjoint_result.Disjoint || disjoint_result.Frequency == 0) {
            RenderGraphProfileSnapshot completed_profile = slot.profile;
            completed_profile.TotalGpuTimeMs = 0.0;
            completed_profile.GpuTimingSupported = true;
            completed_profile.GpuTimingPending = false;
            slot.pending = false;
            m_last_render_graph_profile = std::move(completed_profile);
            LUNA_RENDERER_FRAME_TRACE("Discarded D3D11 GPU timing for frame {} because timestamp disjoint was reported",
                                      frame_index);
            return;
        }

        timestamp_period_ns = 1000000000.0 / static_cast<double>(disjoint_result.Frequency);
    }

    if (timestamp_period_ns <= 0.0) {
        slot.pending = false;
        return;
    }

    RenderGraphProfileSnapshot completed_profile = slot.profile;
    completed_profile.TotalGpuTimeMs = 0.0;
    completed_profile.GpuTimingSupported = true;
    completed_profile.GpuTimingPending = false;

    const size_t pass_count = (std::min) (completed_profile.Passes.size(), timestamps.size() / 2);
    const uint64_t first_timestamp = !timestamps.empty() ? timestamps.front() : 0;
    for (size_t pass_index = 0; pass_index < pass_count; ++pass_index) {
        const uint64_t begin_timestamp = timestamps[pass_index * 2];
        const uint64_t end_timestamp = timestamps[pass_index * 2 + 1];
        if (end_timestamp < begin_timestamp || begin_timestamp < first_timestamp) {
            continue;
        }

        const double gpu_start_ms =
            static_cast<double>(begin_timestamp - first_timestamp) * timestamp_period_ns / 1000000.0;
        const double gpu_time_ms =
            static_cast<double>(end_timestamp - begin_timestamp) * timestamp_period_ns / 1000000.0;
        auto& pass = completed_profile.Passes[pass_index];
        pass.GpuStartMs = gpu_start_ms;
        pass.GpuTimeMs = gpu_time_ms;
        pass.HasGpuTime = true;
        completed_profile.TotalGpuTimeMs += gpu_time_ms;
    }

    slot.pending = false;
    m_last_render_graph_profile = std::move(completed_profile);
}

bool Renderer::storePendingGpuTimingProfile(uint32_t frame_index, const RenderGraphProfileSnapshot& profile)
{
    if (frame_index >= m_frame_resources.gpu_timing_slots.size() || !profile.GpuTimingSupported ||
        !profile.GpuTimingPending) {
        return false;
    }

    auto& slot = m_frame_resources.gpu_timing_slots[frame_index];
    if (!slot.query_pool) {
        return false;
    }

    const uint32_t query_count = static_cast<uint32_t>(profile.Passes.size() * 2);
    if (query_count == 0 || query_count > slot.query_pool->GetCount()) {
        slot.pending = false;
        slot.query_count = 0;
        return false;
    }

    slot.profile = profile;
    slot.query_count = query_count;
    slot.pending = true;
    return true;
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

void Renderer::collectCompletedScenePickResult(SceneViewportState& viewport, uint32_t frame_index)
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
    viewport.output.completed_pick_id = picked_id;
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
