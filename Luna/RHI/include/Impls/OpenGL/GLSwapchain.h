#ifndef CACAO_GLSWAPCHAIN_H
#define CACAO_GLSWAPCHAIN_H
#include "GLCommon.h"
#include "Swapchain.h"

namespace Cacao {
class CACAO_API GLSwapchain final : public Swapchain {
public:
    GLSwapchain(const SwapchainCreateInfo& info);
    static Ref<GLSwapchain> Create(const SwapchainCreateInfo& info);
    ~GLSwapchain() override;

    Result Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex) override;
    uint32_t GetImageCount() const override;
    Ref<Texture> GetBackBuffer(uint32_t index) const override;
    Extent2D GetExtent() const override;
    Format GetFormat() const override;
    PresentMode GetPresentMode() const override;
    Result AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out) override;

private:
    SwapchainCreateInfo m_createInfo;
    uint32_t m_currentIndex = 0;
    void* m_hdc = nullptr;
};
} // namespace Cacao

#endif
