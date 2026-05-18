#pragma once

#include "Renderer/RenderGraphBuilder.h"

#include <Core.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace luna::RHI {
class Buffer;
class CommandBufferEncoder;
class Device;
class QueryPool;
} // namespace luna::RHI

namespace luna {

struct ScenePickReadbackSlot {
    RHI::Ref<RHI::Buffer> buffer;
    bool pending{false};
};

struct GpuTimingSlot {
    RHI::Ref<RHI::QueryPool> query_pool;
    RHI::Ref<RHI::QueryPool> disjoint_query_pool;
    RenderGraphProfileSnapshot profile;
    uint32_t query_count{0};
    bool pending{false};
    bool uses_disjoint_timestamps{false};
};

struct BeginFrameDesc {
    RHI::Ref<RHI::Device> device;
};

struct FrameResourceReleaseStats {
    size_t command_buffer_count{0};
    size_t transient_texture_cache_count{0};
    size_t scene_pick_readback_slot_count{0};
    size_t gpu_timing_slot_count{0};
};

class FrameResourceRing {
public:
    void resize(uint32_t frames_in_flight);
    void beginFrame(uint32_t frame_index, const BeginFrameDesc& desc);
    void releaseFrame(uint32_t frame_index);

    [[nodiscard]] RHI::CommandBufferEncoder* currentCommandBuffer() const;
    [[nodiscard]] const RHI::Ref<RHI::CommandBufferEncoder>& currentCommandBufferRef() const noexcept;
    void resetCurrentCommandBuffer() noexcept;

    [[nodiscard]] RenderGraphTransientTextureCache* transientTextureCache(uint32_t frame_index);
    [[nodiscard]] ScenePickReadbackSlot* pickReadbackSlot(uint32_t frame_index);
    [[nodiscard]] GpuTimingSlot* gpuTimingSlot(uint32_t frame_index);

    [[nodiscard]] luna::RenderGraph* renderGraph(uint32_t frame_index);
    [[nodiscard]] const luna::RenderGraph* renderGraph(uint32_t frame_index) const;
    void setRenderGraph(uint32_t frame_index, std::unique_ptr<luna::RenderGraph> render_graph);

    [[nodiscard]] uint32_t framesInFlight() const noexcept;
    [[nodiscard]] size_t transientTextureCacheCount() const noexcept;
    [[nodiscard]] size_t pickReadbackSlotCount() const noexcept;
    [[nodiscard]] size_t gpuTimingSlotCount() const noexcept;

    void resizePickReadbackSlots(uint32_t frames_in_flight);
    void clearPickReadbackSlots();
    void resizeGpuTimingSlots(uint32_t frames_in_flight);
    void clearGpuTimingSlots();
    FrameResourceReleaseStats releaseAll();

private:
    [[nodiscard]] bool hasFrame(uint32_t frame_index) const noexcept;

private:
    RHI::Ref<RHI::CommandBufferEncoder> m_current_command_buffer;
    std::vector<RHI::Ref<RHI::CommandBufferEncoder>> m_command_buffers;
    std::vector<std::unique_ptr<luna::RenderGraph>> m_render_graphs;
    std::vector<luna::RenderGraphTransientTextureCache> m_transient_texture_caches;
    std::vector<ScenePickReadbackSlot> m_scene_pick_readback_slots;
    std::vector<GpuTimingSlot> m_gpu_timing_slots;
    uint32_t m_frames_in_flight{0};
};

} // namespace luna
