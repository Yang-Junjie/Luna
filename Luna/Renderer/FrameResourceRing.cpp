#include "Renderer/FrameResourceRing.h"

#include <CommandBufferEncoder.h>
#include <Device.h>

namespace luna {

namespace {

const RHI::Ref<RHI::CommandBufferEncoder>& emptyCommandBufferRef()
{
    static const RHI::Ref<RHI::CommandBufferEncoder> empty_ref{};
    return empty_ref;
}

} // namespace

void FrameResourceRing::resize(uint32_t frames_in_flight)
{
    releaseAll();
    m_frames_in_flight = frames_in_flight;
    m_command_buffers.assign(m_frames_in_flight, {});
    m_render_graphs.clear();
    m_render_graphs.resize(m_frames_in_flight);
    m_transient_texture_caches.clear();
    m_transient_texture_caches.resize(m_frames_in_flight);
    m_scene_pick_readback_slots.clear();
    m_gpu_timing_slots.clear();
}

void FrameResourceRing::beginFrame(uint32_t frame_index, const BeginFrameDesc& desc)
{
    if (!hasFrame(frame_index)) {
        return;
    }

    m_transient_texture_caches[frame_index].BeginFrame();
    m_current_command_buffer = desc.device ? desc.device->CreateCommandBufferEncoder() : nullptr;
    if (!m_current_command_buffer) {
        return;
    }

    m_command_buffers[frame_index] = m_current_command_buffer;
    m_current_command_buffer->Begin();
}

void FrameResourceRing::releaseFrame(uint32_t frame_index)
{
    if (!hasFrame(frame_index)) {
        return;
    }

    if (m_command_buffers[frame_index]) {
        m_command_buffers[frame_index]->ReturnToPool();
        if (m_current_command_buffer == m_command_buffers[frame_index]) {
            m_current_command_buffer.reset();
        }
        m_command_buffers[frame_index].reset();
    }

    if (frame_index < m_render_graphs.size()) {
        m_render_graphs[frame_index].reset();
    }
}

RHI::CommandBufferEncoder* FrameResourceRing::currentCommandBuffer() const
{
    return m_current_command_buffer.get();
}

const RHI::Ref<RHI::CommandBufferEncoder>& FrameResourceRing::currentCommandBufferRef() const noexcept
{
    return m_current_command_buffer ? m_current_command_buffer : emptyCommandBufferRef();
}

void FrameResourceRing::resetCurrentCommandBuffer() noexcept
{
    m_current_command_buffer.reset();
}

RenderGraphTransientTextureCache* FrameResourceRing::transientTextureCache(uint32_t frame_index)
{
    return frame_index < m_transient_texture_caches.size() ? &m_transient_texture_caches[frame_index] : nullptr;
}

ScenePickReadbackSlot* FrameResourceRing::pickReadbackSlot(uint32_t frame_index)
{
    return frame_index < m_scene_pick_readback_slots.size() ? &m_scene_pick_readback_slots[frame_index] : nullptr;
}

GpuTimingSlot* FrameResourceRing::gpuTimingSlot(uint32_t frame_index)
{
    return frame_index < m_gpu_timing_slots.size() ? &m_gpu_timing_slots[frame_index] : nullptr;
}

luna::RenderGraph* FrameResourceRing::renderGraph(uint32_t frame_index)
{
    return frame_index < m_render_graphs.size() ? m_render_graphs[frame_index].get() : nullptr;
}

const luna::RenderGraph* FrameResourceRing::renderGraph(uint32_t frame_index) const
{
    return frame_index < m_render_graphs.size() ? m_render_graphs[frame_index].get() : nullptr;
}

void FrameResourceRing::setRenderGraph(uint32_t frame_index, std::unique_ptr<luna::RenderGraph> render_graph)
{
    if (frame_index >= m_render_graphs.size()) {
        m_render_graphs.resize(m_frames_in_flight);
    }
    if (frame_index < m_render_graphs.size()) {
        m_render_graphs[frame_index] = std::move(render_graph);
    }
}

uint32_t FrameResourceRing::framesInFlight() const noexcept
{
    return m_frames_in_flight;
}

size_t FrameResourceRing::transientTextureCacheCount() const noexcept
{
    return m_transient_texture_caches.size();
}

size_t FrameResourceRing::pickReadbackSlotCount() const noexcept
{
    return m_scene_pick_readback_slots.size();
}

size_t FrameResourceRing::gpuTimingSlotCount() const noexcept
{
    return m_gpu_timing_slots.size();
}

void FrameResourceRing::resizePickReadbackSlots(uint32_t frames_in_flight)
{
    m_scene_pick_readback_slots.clear();
    m_scene_pick_readback_slots.resize(frames_in_flight);
}

void FrameResourceRing::clearPickReadbackSlots()
{
    m_scene_pick_readback_slots.clear();
}

void FrameResourceRing::resizeGpuTimingSlots(uint32_t frames_in_flight)
{
    m_gpu_timing_slots.clear();
    m_gpu_timing_slots.resize(frames_in_flight);
}

void FrameResourceRing::clearGpuTimingSlots()
{
    m_gpu_timing_slots.clear();
}

FrameResourceReleaseStats FrameResourceRing::releaseAll()
{
    FrameResourceReleaseStats stats{
        .transient_texture_cache_count = m_transient_texture_caches.size(),
        .scene_pick_readback_slot_count = m_scene_pick_readback_slots.size(),
        .gpu_timing_slot_count = m_gpu_timing_slots.size(),
    };

    for (auto& command_buffer : m_command_buffers) {
        if (command_buffer) {
            command_buffer->ReturnToPool();
            command_buffer.reset();
            ++stats.command_buffer_count;
        }
    }

    m_current_command_buffer.reset();
    m_command_buffers.clear();
    m_render_graphs.clear();
    m_transient_texture_caches.clear();
    m_scene_pick_readback_slots.clear();
    m_gpu_timing_slots.clear();
    m_frames_in_flight = 0;
    return stats;
}

bool FrameResourceRing::hasFrame(uint32_t frame_index) const noexcept
{
    return frame_index < m_frames_in_flight;
}

} // namespace luna
