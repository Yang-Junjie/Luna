#include "Adapter.h"
#include "Impls/Vulkan/VKAdapter.h"
#include "Impls/Vulkan/VKCommon.h"
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKQueue.h"
#include "Impls/Vulkan/VKSurface.h"
#include "Impls/Vulkan/VKSwapchain.h"
#include "Impls/Vulkan/VKSynchronization.h"
#include "Impls/Vulkan/VKTexture.h"
#include "Synchronization.h"

namespace luna::RHI {
Ref<VKSwapchain> VKSwapchain::Create(const Ref<Device>& device, const SwapchainCreateInfo& createInfo)
{
    return CreateRef<VKSwapchain>(device, createInfo);
}

VKSwapchain::VKSwapchain(const Ref<Device>& device, const SwapchainCreateInfo& createInfo)
    : m_device(std::dynamic_pointer_cast<VKDevice>(device)),
      m_swapchainCreateInfo(createInfo)
{
    if (!device) {
        throw std::runtime_error("VKSwapchain created with null device");
    }
    if (!createInfo.CompatibleSurface) {
        throw std::runtime_error("VKSwapchain created with null compatible surface");
    }
    auto pyDevice = m_device->GetHandle();
    vk::SwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.imageArrayLayers = createInfo.ImageArrayLayers;
    switch (createInfo.PresentMode) {
        case PresentMode::Immediate:
            swapchainCreateInfo.presentMode = vk::PresentModeKHR::eImmediate;
            break;
        case PresentMode::Mailbox:
            swapchainCreateInfo.presentMode = vk::PresentModeKHR::eMailbox;
            break;
        case PresentMode::Fifo:
            swapchainCreateInfo.presentMode = vk::PresentModeKHR::eFifo;
            break;
        case PresentMode::FifoRelaxed:
            swapchainCreateInfo.presentMode = vk::PresentModeKHR::eFifoRelaxed;
            break;
        default:
            swapchainCreateInfo.presentMode = vk::PresentModeKHR::eFifo;
            break;
    }
    swapchainCreateInfo.imageFormat = VKConverter::Convert(createInfo.Format);
    switch (createInfo.ColorSpace) {
        case ColorSpace::SRGB_NONLINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            break;
        case ColorSpace::EXTENDED_SRGB_LINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eExtendedSrgbLinearEXT;
            break;
        case ColorSpace::EXTENDED_SRGB_NONLINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT;
            break;
        case ColorSpace::HDR10_ST2084:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eHdr10St2084EXT;
            break;
        case ColorSpace::HDR10_HLG:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eHdr10HlgEXT;
            break;
        case ColorSpace::DOLBY_VISION:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eDolbyvisionEXT;
            break;
        case ColorSpace::ADOBERGB_LINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eAdobergbLinearEXT;
            break;
        case ColorSpace::ADOBERGB_NONLINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eAdobergbNonlinearEXT;
            break;
        case ColorSpace::DISPLAY_P3_NONLINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eDisplayP3NonlinearEXT;
            break;
        case ColorSpace::DISPLAY_P3_LINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eDisplayP3LinearEXT;
            break;
        case ColorSpace::DCI_P3_NONLINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eDciP3NonlinearEXT;
            break;
        case ColorSpace::BT709_LINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eBt709LinearEXT;
            break;
        case ColorSpace::BT709_NONLINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eBt709NonlinearEXT;
            break;
        case ColorSpace::BT2020_LINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eBt2020LinearEXT;
            break;
        case ColorSpace::BT2020_NONLINEAR:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eHdr10St2084EXT;
            break;
        case ColorSpace::PASS_THROUGH:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::ePassThroughEXT;
            break;
        default:
            swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            break;
    }
    switch (createInfo.CompositeAlpha) {
        case CompositeAlpha::Opaque:
            swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            break;
        case CompositeAlpha::Inherit:
            swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
            break;
        case CompositeAlpha::PreMultiplied:
            swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
            break;
        case CompositeAlpha::PostMultiplied:
            swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
            break;
        default:
            swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            break;
    }
    vk::ImageUsageFlags imageUsage;
    if (createInfo.Usage & SwapchainUsageFlags::ColorAttachment) {
        imageUsage |= vk::ImageUsageFlagBits::eColorAttachment;
    }
    if (createInfo.Usage & SwapchainUsageFlags::TransferSrc) {
        imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    if (createInfo.Usage & SwapchainUsageFlags::TransferDst) {
        imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
    }
    if (createInfo.Usage & SwapchainUsageFlags::Storage) {
        imageUsage |= vk::ImageUsageFlagBits::eStorage;
    }
    swapchainCreateInfo.imageUsage = imageUsage;
    swapchainCreateInfo.minImageCount = createInfo.MinImageCount;
    swapchainCreateInfo.imageExtent = vk::Extent2D{createInfo.Extent.width, createInfo.Extent.height};
    vk::SurfaceTransformFlagBitsKHR preTransform;
    switch (createInfo.PreTransform.rotation) {
        case SurfaceRotation::Identity:
            preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
            break;
        case SurfaceRotation::Rotate90:
            preTransform = vk::SurfaceTransformFlagBitsKHR::eRotate90;
            break;
        case SurfaceRotation::Rotate180:
            preTransform = vk::SurfaceTransformFlagBitsKHR::eRotate180;
            break;
        case SurfaceRotation::Rotate270:
            preTransform = vk::SurfaceTransformFlagBitsKHR::eRotate270;
            break;
        default:
            preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
            break;
    }
    swapchainCreateInfo.preTransform = preTransform;
    swapchainCreateInfo.clipped = createInfo.Clipped;
    std::vector<uint32_t> queueFamilyIndices;
    vk::SharingMode sharingMode;
    uint32_t graphicsFamily = m_device->GetParentAdapter()->FindQueueFamilyIndex(QueueType::Graphics);
    uint32_t presentFamily = std::dynamic_pointer_cast<VKSurface>(createInfo.CompatibleSurface)
                                 ->GetPresentQueueFamilyIndex(device->GetParentAdapter());
    if (graphicsFamily != presentFamily) {
        sharingMode = vk::SharingMode::eConcurrent;
        queueFamilyIndices.push_back(graphicsFamily);
        queueFamilyIndices.push_back(presentFamily);
        swapchainCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        sharingMode = vk::SharingMode::eExclusive;
    }
    swapchainCreateInfo.imageSharingMode = sharingMode;
    swapchainCreateInfo.surface =
        std::dynamic_pointer_cast<VKSurface>(createInfo.CompatibleSurface)->GetVulkanSurface();
    m_swapchain = pyDevice.createSwapchainKHR(swapchainCreateInfo);
    if (!m_swapchain) {
        throw std::runtime_error("Failed to create Vulkan swapchain");
    }
    printf("VK Swapchain: format=%d, colorSpace=%d\n",
           (int) swapchainCreateInfo.imageFormat,
           (int) swapchainCreateInfo.imageColorSpace);
    m_images = pyDevice.getSwapchainImagesKHR(m_swapchain);
    for (const auto& image : m_images) {
        vk::ImageViewCreateInfo ivci{};
        ivci.image = image;
        ivci.viewType = vk::ImageViewType::e2D;
        ivci.format = swapchainCreateInfo.imageFormat;
        ivci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        vk::ImageView imageView = pyDevice.createImageView(ivci);
        m_imageViews.push_back(imageView);
    }

    CreateBackBuffers();
}

Result VKSwapchain::Present(const Ref<Queue>& queue, const Ref<Synchronization>& sync, uint32_t frameIndex)
{
    if (!m_hasAcquiredImage) {
        return Result::Error;
    }

    vk::PresentInfoKHR present{};
    auto syncContext = std::dynamic_pointer_cast<VKSynchronization>(sync);
    auto vkQueue = std::static_pointer_cast<VKQueue>(queue);
    uint32_t imageIndex = m_currentImageIndex;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &syncContext->GetRenderSemaphore(frameIndex);
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &imageIndex;
    try {
        auto res = vkQueue->GetVulkanQueue().presentKHR(present);
        m_hasAcquiredImage = false;
        switch (res) {
            case vk::Result::eSuccess:
                return Result::Success;
            case vk::Result::eSuboptimalKHR:
                return Result::Suboptimal;
            case vk::Result::eTimeout:
                return Result::Timeout;
            case vk::Result::eNotReady:
                return Result::NotReady;
            case vk::Result::eErrorOutOfDateKHR:
                return Result::OutOfDate;
            case vk::Result::eErrorDeviceLost:
                return Result::DeviceLost;
            default:
                return Result::Error;
        }
    } catch (vk::OutOfDateKHRError) {
        m_hasAcquiredImage = false;
        return Result::OutOfDate;
    } catch (vk::DeviceLostError) {
        m_hasAcquiredImage = false;
        return Result::DeviceLost;
    } catch (...) {
        m_hasAcquiredImage = false;
        return Result::Error;
    }
}

uint32_t VKSwapchain::GetImageCount() const
{
    return m_images.size();
}

void VKSwapchain::CreateBackBuffers()
{
    m_backBuffers.clear();
    m_backBuffers.reserve(m_images.size());

    TextureCreateInfo info{};
    info.Type = TextureType::Texture2D;
    info.Width = m_swapchainCreateInfo.Extent.width;
    info.Height = m_swapchainCreateInfo.Extent.height;
    info.Depth = 1;
    info.ArrayLayers = 1;
    info.MipLevels = 1;
    info.Format = m_swapchainCreateInfo.Format;

    TextureUsageFlags usage = {};
    if (m_swapchainCreateInfo.Usage & SwapchainUsageFlags::ColorAttachment) {
        usage |= TextureUsageFlags::ColorAttachment;
    }
    if (m_swapchainCreateInfo.Usage & SwapchainUsageFlags::TransferSrc) {
        usage |= TextureUsageFlags::TransferSrc;
    }
    if (m_swapchainCreateInfo.Usage & SwapchainUsageFlags::TransferDst) {
        usage |= TextureUsageFlags::TransferDst;
    }
    if (m_swapchainCreateInfo.Usage & SwapchainUsageFlags::Storage) {
        usage |= TextureUsageFlags::Storage;
    }

    info.Usage = usage;
    info.InitialState = ResourceState::Present;
    info.SampleCount = SampleCount::Count1;
    info.Name = "SwapchainBackBuffer";
    info.InitialData = nullptr;

    for (size_t i = 0; i < m_images.size(); ++i) {
        m_backBuffers.push_back(VKTexture::CreateFromSwapchainImage(m_device, m_images[i], m_imageViews[i], info));
    }
}

Ref<Texture> VKSwapchain::GetBackBuffer(uint32_t index) const
{
    if (index < m_backBuffers.size()) {
        return m_backBuffers[index];
    }
    return nullptr;
}

Extent2D VKSwapchain::GetExtent() const
{
    return m_swapchainCreateInfo.Extent;
}

Format VKSwapchain::GetFormat() const
{
    return m_swapchainCreateInfo.Format;
}

PresentMode VKSwapchain::GetPresentMode() const
{
    return m_swapchainCreateInfo.PresentMode;
}

Result VKSwapchain::AcquireNextImage(const Ref<Synchronization>& sync, int idx, int& out)
{
    if (!sync) {
        return Result::Error;
    }
    out = static_cast<int>(sync->AcquireNextImageIndex(shared_from_this(), idx));
    m_currentImageIndex = static_cast<uint32_t>(out);
    m_hasAcquiredImage = true;
    return Result::Success;
}

VKSwapchain::~VKSwapchain()
{
    if (m_device) {
        auto deviceHandle = m_device->GetHandle();
        m_backBuffers.clear();
        for (auto& imageView : m_imageViews) {
            if (imageView) {
                deviceHandle.destroyImageView(imageView);
            }
        }
        m_imageViews.clear();
        m_images.clear();
        if (m_swapchain) {
            deviceHandle.destroySwapchainKHR(m_swapchain);
            m_swapchain = nullptr;
        }
    }
}
} // namespace luna::RHI
