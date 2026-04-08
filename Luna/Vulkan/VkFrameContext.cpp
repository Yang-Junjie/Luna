#include "VkFrameContext.h"

#include "VkInitializers.h"
#include "VkVMAAllocator.h"

namespace luna::vkcore {

bool FrameContext::initialize(vk::Device device,
                              const VMAAllocator& allocator,
                              uint32_t queue_family_index,
                              uint32_t descriptor_set_count,
                              std::span<DescriptorAllocatorGrowable::PoolSizeRatio> descriptor_pool_ratios,
                              vk::DeviceSize uniform_arena_size,
                              vk::DeviceSize staging_arena_size)
{
    destroy(device);

    vk::CommandPoolCreateInfo command_pool_info =
        vkinit::commandPoolCreateInfo(queue_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(device.createCommandPool(&command_pool_info, nullptr, &m_command_pool));

    vk::CommandBufferAllocateInfo cmd_alloc_info = vkinit::commandBufferAllocateInfo(m_command_pool, 1);
    VK_CHECK(device.allocateCommandBuffers(&cmd_alloc_info, &m_main_command_buffer));

    vk::FenceCreateInfo fence_create_info = vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    vk::SemaphoreCreateInfo semaphore_create_info = vkinit::semaphoreCreateInfo();
    VK_CHECK(device.createFence(&fence_create_info, nullptr, &m_render_fence));
    VK_CHECK(device.createSemaphore(&semaphore_create_info, nullptr, &m_swapchain_semaphore));
    VK_CHECK(device.createSemaphore(&semaphore_create_info, nullptr, &m_render_semaphore));

    m_frame_descriptors.init(device, descriptor_set_count, descriptor_pool_ratios);

    if (!m_uniform_arena.init(allocator,
                              uniform_arena_size,
                              vk::BufferUsageFlagBits::eUniformBuffer,
                              VMA_MEMORY_USAGE_CPU_TO_GPU)) {
        destroy(device);
        return false;
    }

    if (!m_staging_arena.init(allocator,
                              staging_arena_size,
                              vk::BufferUsageFlagBits::eTransferSrc,
                              VMA_MEMORY_USAGE_CPU_ONLY)) {
        destroy(device);
        return false;
    }

    return true;
}

void FrameContext::destroy(vk::Device device)
{
    m_uniform_arena.destroy();
    m_staging_arena.destroy();
    m_frame_descriptors.destroyPools(device);

    if (m_command_pool != VK_NULL_HANDLE) {
        device.destroyCommandPool(m_command_pool, nullptr);
        m_command_pool = VK_NULL_HANDLE;
        m_main_command_buffer = VK_NULL_HANDLE;
    }

    if (m_render_fence != VK_NULL_HANDLE) {
        device.destroyFence(m_render_fence, nullptr);
        m_render_fence = VK_NULL_HANDLE;
    }

    if (m_render_semaphore != VK_NULL_HANDLE) {
        device.destroySemaphore(m_render_semaphore, nullptr);
        m_render_semaphore = VK_NULL_HANDLE;
    }

    if (m_swapchain_semaphore != VK_NULL_HANDLE) {
        device.destroySemaphore(m_swapchain_semaphore, nullptr);
        m_swapchain_semaphore = VK_NULL_HANDLE;
    }
}

void FrameContext::beginFrame(vk::Device device, DeferredRelease& deferred_release, uint64_t frame_number)
{
    VK_CHECK(device.waitForFences(1, &m_render_fence, VK_TRUE, 1'000'000'000));
    deferred_release.flush(frame_number);
    VK_CHECK(device.resetCommandPool(m_command_pool, {}));

    m_frame_descriptors.clearPools(device);
    m_uniform_arena.reset();
    m_staging_arena.reset();
}

} // namespace luna::vkcore
