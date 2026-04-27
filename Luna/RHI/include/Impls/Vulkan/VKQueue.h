#ifndef LUNA_RHI_VKQUEUE_H
#define LUNA_RHI_VKQUEUE_H
#include "Queue.h"

#include <mutex>
#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class Device;
}

namespace luna::RHI {
class LUNA_RHI_API VKQueue final : public Queue {
    vk::Queue m_queue;
    Ref<Device> m_device;
    QueueType m_type;
    uint32_t m_index;
    std::mutex m_submitMutex;
    friend class VKSwapchain;

    vk::Queue& GetVulkanQueue()
    {
        return m_queue;
    }

public:
    VKQueue(const Ref<Device>& device, const vk::Queue& vkQueue, QueueType type, uint32_t index);
    static Ref<VKQueue> Create(const Ref<Device>& device, const vk::Queue& vkQueue, QueueType type, uint32_t index);
    QueueType GetType() const override;
    uint32_t GetIndex() const override;
    void Submit(const Ref<CommandBufferEncoder>& cmd, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
    void Submit(std::span<const Ref<CommandBufferEncoder>> cmds,
                const Ref<Synchronization>& sync,
                uint32_t frameIndex) override;
    void Submit(const Ref<CommandBufferEncoder>& cmd) override;
    void WaitIdle() override;
    double GetTimestampPeriodNs() const override;

    const vk::Queue& GetNativeHandle() const
    {
        return m_queue;
    }
};
} // namespace luna::RHI
#endif
