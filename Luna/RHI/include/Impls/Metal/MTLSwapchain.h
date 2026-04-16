#ifndef CACAO_MTLSWAPCHAIN_H
#define CACAO_MTLSWAPCHAIN_H
#ifdef __APPLE__
#include "MTLCommon.h"
#include "Swapchain.h"

namespace Cacao {
class CACAO_API MTLSwapchain final : public Swapchain {
public:
    MTLSwapchain(const Ref<Device>& device, const SwapchainCreateInfo& info);
    ~MTLSwapchain() override = default;

    Result Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
    Result AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out) override;
    uint32_t GetImageCount() const override;
    Ref<Texture> GetBackBuffer(uint32_t index) const override;
    Extent2D GetExtent() const override;
    Format GetFormat() const override;
    PresentMode GetPresentMode() const override;

private:
    SwapchainCreateInfo m_createInfo;
    id m_metalLayer = nullptr; // CAMetalLayer*
};
} // namespace Cacao
#endif // __APPLE__
#endif
