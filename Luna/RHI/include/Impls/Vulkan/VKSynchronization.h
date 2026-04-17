#ifndef LUNA_RHI_VKSYNCHRONIZATION_H
#define LUNA_RHI_VKSYNCHRONIZATION_H
#include "Device.h"
#include "Synchronization.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class VKDevice;

class LUNA_RHI_API VKSynchronization : public Synchronization {
    uint32_t m_maxFramesInFlight;
    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;
    std::vector<uint32_t> m_frameImageIndices;
    Ref<VKDevice> m_vkDevice;
    friend class VKDevice;
    friend class VKSwapchain;
    friend class VKQueue;
    void EnsureRenderSemaphore(uint32_t imageIndex);
    vk::Semaphore& GetImageSemaphore(uint32_t frameIndex);
    vk::Semaphore& GetRenderSemaphore(uint32_t frameIndex);
    vk::Fence& GetInFlightFence(uint32_t frameIndex);

public:
    static Ref<VKSynchronization> Create(const Ref<Device>& device, uint32_t maxFramesInFlight);
    VKSynchronization(const Ref<Device>& device, uint32_t maxFramesInFlight);
    void WaitForFrame(uint32_t frameIndex) const override;
    void ResetFrameFence(uint32_t frameIndex) const override;
    uint32_t AcquireNextImageIndex(const Ref<Swapchain>& swapchain, uint32_t frameIndex) const override;
    uint32_t GetMaxFramesInFlight() const override;
    ~VKSynchronization() override;
    void WaitIdle() const override;
};
} // namespace luna::RHI
#endif
