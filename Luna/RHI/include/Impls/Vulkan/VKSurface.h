#ifndef LUNA_RHI_VKSURFACE_H
#define LUNA_RHI_VKSURFACE_H
#include "Surface.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class Device;
class VKQueue;
} // namespace luna::RHI

namespace luna::RHI {
class LUNA_RHI_API VKSurface : public Surface {
    vk::SurfaceKHR m_surface;
    SurfaceCapabilities m_surfaceCapabilities;
    std::vector<SurfaceFormat> m_surfaceFormats;
    std::vector<PresentMode> m_presentModes;
    friend class VKSwapchain;

    vk::SurfaceKHR& GetVulkanSurface()
    {
        return m_surface;
    }

public:
    VKSurface(const vk::SurfaceKHR& surface);
    SurfaceCapabilities GetCapabilities(const Ref<Adapter>& adapter) override;
    std::vector<SurfaceFormat> GetSupportedFormats(const Ref<Adapter>& adapter) override;
    std::vector<PresentMode> GetSupportedPresentModes(const Ref<Adapter>& adapter) override;
    uint32_t GetPresentQueueFamilyIndex(const Ref<Adapter>& adapter) const;
    Ref<Queue> GetPresentQueue(const Ref<Device>& device) override;
};
} // namespace luna::RHI
#endif
