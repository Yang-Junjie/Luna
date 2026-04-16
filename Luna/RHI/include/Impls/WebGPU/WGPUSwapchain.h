#ifndef CACAO_WGPU_SWAPCHAIN_H
#define CACAO_WGPU_SWAPCHAIN_H

#include "Swapchain.h"

#include <webgpu/webgpu.h>

namespace Cacao {
class WGPUDevice;

class CACAO_API WGPUSwapchain : public Swapchain {
private:
    Ref<WGPUDevice> m_device;
    SwapchainCreateInfo m_createInfo;
    ::WGPUSurface m_surface = nullptr;
    Ref<Texture> m_currentBackBuffer;
    uint32_t m_imageCount = 2;

public:
    WGPUSwapchain(const Ref<Device>& device, const SwapchainCreateInfo& info);
    ~WGPUSwapchain() override;

    Extent2D GetExtent() const override;
    Format GetFormat() const override;
    PresentMode GetPresentMode() const override;
    uint32_t GetImageCount() const override;
    Result AcquireNextImage(const Ref<Synchronization>& sync, int frameIndex, int& imageIndex) override;
    Result Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
    Ref<Texture> GetBackBuffer(uint32_t index) const override;
};
} // namespace Cacao

#endif
