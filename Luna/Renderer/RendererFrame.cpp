#include "Core/Log.h"
#include "Imgui/ImGuiContext.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererInternal.h"

#include <array>
#include <CommandBufferEncoder.h>
#include <Device.h>
#include <Queue.h>
#include <stdexcept>
#include <Swapchain.h>
#include <Synchronization.h>

namespace luna {

namespace {

bool supportsScenePickReadback(luna::RHI::BackendType backend_type)
{
    return backend_type == luna::RHI::BackendType::Vulkan || backend_type == luna::RHI::BackendType::DirectX12;
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
        collectCompletedScenePickResult(frame_resources.frame_index);

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
    }
}

void Renderer::renderFrame()
{
    auto& device_context = m_device_context;
    auto& frame_resources = m_frame_resources;
    auto& runtime = m_runtime;
    auto& scene_output = m_scene_output;

    if (!runtime.frame_started || !frame_resources.current_command_buffer || !device_context.swapchain) {
        LUNA_RENDERER_FRAME_TRACE(
            "Skipping RenderFrame because the frame has not started or swapchain state is incomplete");
        return;
    }

    const auto extent = device_context.swapchain->GetExtent();
    const auto back_buffer = device_context.swapchain->GetBackBuffer(frame_resources.image_index);
    const bool was_presented = frame_resources.image_index < frame_resources.swapchain_images_presented.size()
                                   ? frame_resources.swapchain_images_presented[frame_resources.image_index]
                                   : false;
    const auto backend_type =
        device_context.instance ? device_context.instance->GetType() : runtime.initialization_options.backend;
    const bool offscreen_scene_output = scene_output.mode == SceneOutputMode::OffscreenTexture;
    LUNA_RENDERER_FRAME_DEBUG(
        "Building frame {} render graph: swapchain_extent={}x{} backend={} scene_output_mode={} imgui={}",
        frame_resources.frame_index,
        extent.width,
        extent.height,
        renderer_detail::backendTypeToString(backend_type),
        offscreen_scene_output ? "OffscreenTexture" : "Swapchain",
        runtime.imgui_enabled);

    const bool render_scene_to_offscreen = offscreen_scene_output && scene_output.extent.width > 0 &&
                                           scene_output.extent.height > 0 && scene_output.color && scene_output.depth &&
                                           scene_output.pick;
    const bool render_scene_to_swapchain = !offscreen_scene_output;
    luna::rhi::RenderGraphTransientTextureCache* transient_texture_cache =
        (frame_resources.frame_index < frame_resources.transient_texture_caches.size())
            ? &frame_resources.transient_texture_caches[frame_resources.frame_index]
            : nullptr;
    luna::rhi::RenderGraphBuilder graph_builder(
        luna::rhi::RenderGraphBuilder::FrameContext{
            .device = device_context.device,
            .command_buffer = frame_resources.current_command_buffer,
            .framebuffer_width = extent.width,
            .framebuffer_height = extent.height,
        },
        transient_texture_cache);

    const auto back_buffer_handle = graph_builder.ImportTexture("SwapchainBackBuffer",
                                                                back_buffer,
                                                                was_presented ? luna::RHI::ResourceState::Present
                                                                              : luna::RHI::ResourceState::Undefined);

    luna::rhi::RenderGraphTextureHandle scene_color_handle{};
    luna::rhi::RenderGraphTextureHandle scene_depth_handle{};
    luna::rhi::RenderGraphTextureHandle scene_pick_handle{};
    luna::RHI::Format scene_color_format = device_context.surface_format;
    uint32_t scene_width = 0;
    uint32_t scene_height = 0;

    if (render_scene_to_swapchain) {
        scene_color_handle = back_buffer_handle;
        scene_color_format = device_context.surface_format;
        scene_width = extent.width;
        scene_height = extent.height;

        if (renderer_detail::usesSceneRenderer(backend_type)) {
            scene_depth_handle = graph_builder.CreateTexture(luna::rhi::RenderGraphTextureDesc{
                .Name = "SceneDepthTexture",
                .Width = extent.width,
                .Height = extent.height,
                .Format = luna::RHI::Format::D32_FLOAT,
                .Usage = luna::RHI::TextureUsageFlags::DepthStencilAttachment,
                .InitialState = luna::RHI::ResourceState::Undefined,
                .SampleCount = luna::RHI::SampleCount::Count1,
            });
            scene_pick_handle = graph_builder.CreateTexture(luna::rhi::RenderGraphTextureDesc{
                .Name = "ScenePickTexture",
                .Width = extent.width,
                .Height = extent.height,
                .Format = luna::RHI::Format::R32_UINT,
                .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
                .InitialState = luna::RHI::ResourceState::Undefined,
                .SampleCount = luna::RHI::SampleCount::Count1,
            });
        }
    } else if (render_scene_to_offscreen) {
        scene_color_handle = graph_builder.ImportTexture(
            "SceneOutputColor", scene_output.color, scene_output.color_state, luna::RHI::ResourceState::ShaderRead);
        scene_depth_handle = graph_builder.ImportTexture(
            "SceneOutputDepth", scene_output.depth, scene_output.depth_state, luna::RHI::ResourceState::Common);
        scene_pick_handle = graph_builder.ImportTexture(
            "SceneOutputPick", scene_output.pick, scene_output.pick_state, luna::RHI::ResourceState::Common);
        scene_color_format = scene_output.color ? scene_output.color->GetFormat() : device_context.surface_format;
        scene_width = scene_output.color ? scene_output.color->GetWidth() : 0;
        scene_height = scene_output.color ? scene_output.color->GetHeight() : 0;
    }

    const bool scene_output_valid = scene_color_handle.isValid() && scene_width > 0 && scene_height > 0;
    if (!scene_output_valid) {
        LUNA_RENDERER_FRAME_DEBUG("Scene output is invalid for frame {}: color_handle={} size={}x{}",
                                  frame_resources.frame_index,
                                  scene_color_handle.isValid(),
                                  scene_width,
                                  scene_height);
    }

    const bool pick_readback_supported = supportsScenePickReadback(backend_type);
    const bool scene_pick_available = scene_pick_handle.isValid();
    const bool issue_pick_readback = render_scene_to_offscreen && scene_output_valid && scene_pick_available &&
                                     pick_readback_supported && scene_output.queued_pick_request.has_value() &&
                                     frame_resources.frame_index < frame_resources.scene_pick_readback_slots.size() &&
                                     frame_resources.scene_pick_readback_slots[frame_resources.frame_index].buffer;
    if (scene_output_valid) {
        if (renderer_detail::usesSceneRenderer(backend_type) && scene_depth_handle.isValid()) {
            m_scene_renderer.buildRenderGraph(
                graph_builder,
                SceneRenderer::RenderContext{
                    .device = device_context.device,
                    .compiler = device_context.shader_compiler,
                    .backend_type = backend_type,
                    .color_target = scene_color_handle,
                    .depth_target = scene_depth_handle,
                    .pick_target = scene_pick_handle,
                    .color_format = scene_color_format,
                    .clear_color = runtime.clear_color,
                    .show_pick_debug_visualization = scene_output.pick_debug_visualization_enabled,
                    .debug_pick_pixel_x = scene_output.debug_pick_marker.x,
                    .debug_pick_pixel_y = scene_output.debug_pick_marker.y,
                    .show_pick_debug_marker =
                        scene_output.pick_debug_visualization_enabled && scene_output.debug_pick_marker.valid,
                    .framebuffer_width = scene_width,
                    .framebuffer_height = scene_height,
                });
        } else {
            graph_builder.AddRasterPass(
                "ClearScene",
                [scene_color_handle,
                 clear_color = runtime.clear_color](luna::rhi::RenderGraphRasterPassBuilder& pass_builder) {
                    pass_builder.WriteColor(
                        scene_color_handle,
                        luna::RHI::AttachmentLoadOp::Clear,
                        luna::RHI::AttachmentStoreOp::Store,
                        luna::RHI::ClearValue::ColorFloat(clear_color.r, clear_color.g, clear_color.b, clear_color.a));
                },
                [](luna::rhi::RenderGraphRasterPassContext& pass_context) {
                    pass_context.beginRendering();
                    pass_context.endRendering();
                });
        }
    }

    if (render_scene_to_offscreen && scene_output_valid) {
        graph_builder.ExportTexture(scene_color_handle, luna::RHI::ResourceState::ShaderRead);
        if (scene_depth_handle.isValid()) {
            graph_builder.ExportTexture(scene_depth_handle, luna::RHI::ResourceState::Common);
        }
        if (scene_pick_handle.isValid()) {
            graph_builder.ExportTexture(scene_pick_handle,
                                        issue_pick_readback ? luna::RHI::ResourceState::CopySource
                                                            : luna::RHI::ResourceState::Common);
        }
    }

    if (runtime.imgui_enabled) {
        graph_builder.AddRasterPass(
            "ImGui",
            [back_buffer_handle,
             scene_color_handle,
             render_scene_to_offscreen,
             render_scene_to_swapchain,
             scene_output_valid,
             clear_color = runtime.clear_color](luna::rhi::RenderGraphRasterPassBuilder& pass_builder) {
                if (render_scene_to_offscreen && scene_output_valid) {
                    pass_builder.ReadTexture(scene_color_handle);
                }

                pass_builder.WriteColor(
                    back_buffer_handle,
                    render_scene_to_swapchain && scene_output_valid ? luna::RHI::AttachmentLoadOp::Load
                                                                    : luna::RHI::AttachmentLoadOp::Clear,
                    luna::RHI::AttachmentStoreOp::Store,
                    luna::RHI::ClearValue::ColorFloat(clear_color.r, clear_color.g, clear_color.b, clear_color.a));
            },
            [this, back_buffer_handle](luna::rhi::RenderGraphRasterPassContext& pass_context) {
                pass_context.beginRendering();
                luna::rhi::ImGuiRhiContext::RenderDrawData(pass_context.commandBuffer(),
                                                           pass_context.getTexture(back_buffer_handle),
                                                           pass_context.framebufferWidth(),
                                                           pass_context.framebufferHeight(),
                                                           m_frame_resources.frame_index);
                pass_context.endRendering();
            });
    } else if (!render_scene_to_swapchain) {
        graph_builder.AddRasterPass(
            "PresentClear",
            [back_buffer_handle,
             clear_color = runtime.clear_color](luna::rhi::RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(
                    back_buffer_handle,
                    luna::RHI::AttachmentLoadOp::Clear,
                    luna::RHI::AttachmentStoreOp::Store,
                    luna::RHI::ClearValue::ColorFloat(clear_color.r, clear_color.g, clear_color.b, clear_color.a));
            },
            [](luna::rhi::RenderGraphRasterPassContext& pass_context) {
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
    } else {
        LUNA_RENDERER_WARN("Render graph build returned null for frame {}", frame_resources.frame_index);
    }

    if (render_scene_to_offscreen && scene_output_valid && scene_output.queued_pick_request.has_value() &&
        !pick_readback_supported) {
        LUNA_RENDERER_WARN("Scene pick readback is not supported on backend '{}'; dropping pick request",
                           renderer_detail::backendTypeToString(backend_type));
        scene_output.queued_pick_request.reset();
    }

    if (issue_pick_readback) {
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

    if (render_scene_to_offscreen && scene_output_valid) {
        scene_output.color_state = luna::RHI::ResourceState::ShaderRead;
        scene_output.depth_state =
            scene_depth_handle.isValid() ? luna::RHI::ResourceState::Common : luna::RHI::ResourceState::Undefined;
        scene_output.pick_state =
            scene_pick_handle.isValid()
                ? (issue_pick_readback ? luna::RHI::ResourceState::CopySource : luna::RHI::ResourceState::Common)
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
    }

    m_scene_renderer.clearSubmittedMeshes();
    frame_resources.current_command_buffer.reset();
    runtime.frame_started = false;
    if (frame_resources.frames_in_flight > 0) {
        frame_resources.frame_index = (frame_resources.frame_index + 1) % frame_resources.frames_in_flight;
    }
    LUNA_RENDERER_FRAME_TRACE("Advanced to frame index {}", frame_resources.frame_index);
}

} // namespace luna
