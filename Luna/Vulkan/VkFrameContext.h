#pragma once

#include "VkDeferredRelease.h"
#include "VkDescriptors.h"
#include "VkTransientArena.h"

#include <span>

namespace luna::vkcore {

class VMAAllocator;

class FrameContext {
public:
    vk::Semaphore m_swapchain_semaphore{};
    vk::Semaphore m_render_semaphore{};
    vk::Fence m_render_fence{};

    vk::CommandPool m_command_pool{};
    vk::CommandBuffer m_main_command_buffer{};
    DescriptorAllocatorGrowable m_frame_descriptors;
    TransientArena m_uniform_arena;
    TransientArena m_staging_arena;

    bool initialize(vk::Device device,
                    const VMAAllocator& allocator,
                    uint32_t queue_family_index,
                    uint32_t descriptor_set_count,
                    std::span<DescriptorAllocatorGrowable::PoolSizeRatio> descriptor_pool_ratios,
                    vk::DeviceSize uniform_arena_size,
                    vk::DeviceSize staging_arena_size);
    void destroy(vk::Device device);

    void beginFrame(vk::Device device, DeferredRelease& deferred_release, uint64_t frame_number);
};

} // namespace luna::vkcore
