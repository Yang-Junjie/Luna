#ifndef CACAO_VKSWAPCHAIN_H
#define CACAO_VKSWAPCHAIN_H
#include <Swapchain.h>
#include <vulkan/vulkan.hpp>
namespace Cacao
{
    class VKQueue;
    class VKDevice;
    class Device;
    class CACAO_API VKSwapchain : public Swapchain
    {
        vk::SwapchainKHR m_swapchain;
        friend class VKSynchronization;
        vk::SwapchainKHR& GetVulkanSwapchain() { return m_swapchain; }
        std::vector<vk::Image> m_images;
        std::vector<vk::ImageView> m_imageViews;
        Ref<VKDevice> m_device;
        SwapchainCreateInfo m_swapchainCreateInfo;
        uint32_t m_currentImageIndex = 0;
        bool m_hasAcquiredImage = false;
    public:
        static Ref<VKSwapchain> Create(const Ref<Device>& device, const SwapchainCreateInfo& createInfo);
        VKSwapchain(const Ref<Device>& device, const SwapchainCreateInfo& createInfo);
        Result Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync,
                       uint32_t frameIndex) override;
        uint32_t GetImageCount() const override;
        Ref<Texture> GetBackBuffer(uint32_t index) const override;
        Extent2D GetExtent() const override;
        Format GetFormat() const override;
        PresentMode GetPresentMode() const override;
        Result AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out) override;
        ~VKSwapchain() override;
    };
} 
#endif 
