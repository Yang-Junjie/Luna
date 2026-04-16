#include <Impls/Vulkan/VKSurface.h>
#include <Impls/Vulkan/VKAdapter.h>
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKQueue.h"
namespace Cacao
{
    VKSurface::VKSurface(const vk::SurfaceKHR& surface) : m_surface(surface)
    {
        if (!m_surface)
        {
            throw std::runtime_error("Invalid surface provided to VKSurface");
        }
    }
    SurfaceCapabilities VKSurface::GetCapabilities(const Ref<Adapter>& adapter)
    {
        if (!adapter)
        {
            throw std::runtime_error("无法获取 Surface 能力：传入的 Adapter 为空 (Adapter is null)。");
        }
        auto vkAdapter = std::dynamic_pointer_cast<VKAdapter>(adapter);
        if (!vkAdapter)
        {
            throw std::runtime_error("类型转换失败：传入的 Adapter 不是 VKAdapter 实例。");
        }
        vk::PhysicalDevice physicalDevice = vkAdapter->GetPhysicalDevice();
        vk::SurfaceCapabilitiesKHR vkCaps = physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
        m_surfaceCapabilities.minImageCount = vkCaps.minImageCount;
        m_surfaceCapabilities.maxImageCount = vkCaps.maxImageCount;
        m_surfaceCapabilities.currentExtent.width = vkCaps.currentExtent.width;
        m_surfaceCapabilities.currentExtent.height = vkCaps.currentExtent.height;
        m_surfaceCapabilities.minImageExtent.width = vkCaps.minImageExtent.width;
        m_surfaceCapabilities.minImageExtent.height = vkCaps.minImageExtent.height;
        m_surfaceCapabilities.maxImageExtent.width = vkCaps.maxImageExtent.width;
        m_surfaceCapabilities.maxImageExtent.height = vkCaps.maxImageExtent.height;
        auto& transform = m_surfaceCapabilities.currentTransform;
        vk::SurfaceTransformFlagBitsKHR current = vkCaps.currentTransform;
        transform = SurfaceTransform{};
        if (current == vk::SurfaceTransformFlagBitsKHR::eIdentity)
        {
            transform.rotation = SurfaceRotation::Identity;
        }
        else
        {
            if ((current & vk::SurfaceTransformFlagBitsKHR::eRotate90) ==
                vk::SurfaceTransformFlagBitsKHR::eRotate90)
            {
                transform.rotation = SurfaceRotation::Rotate90;
            }
            else if ((current & vk::SurfaceTransformFlagBitsKHR::eRotate180) ==
                vk::SurfaceTransformFlagBitsKHR::eRotate180)
            {
                transform.rotation = SurfaceRotation::Rotate180;
            }
            else if ((current & vk::SurfaceTransformFlagBitsKHR::eRotate270) ==
                vk::SurfaceTransformFlagBitsKHR::eRotate270)
            {
                transform.rotation = SurfaceRotation::Rotate270;
            }
            transform.flipHorizontal = (current & vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror) ==
                vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror;
            bool isHorizontalMirrorRotate180 = (current & vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate180)
                == vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate180;
            if (isHorizontalMirrorRotate180)
            {
                transform.flipVertical = true;
            }
        }
        return m_surfaceCapabilities;
    }
    std::vector<SurfaceFormat> VKSurface::GetSupportedFormats(const Ref<Adapter>& adapter)
    {
        if (!adapter)
        {
            throw std::runtime_error("Adapter is null in VKSurface::GetCapabilities");
        }
        auto pyDevice = std::dynamic_pointer_cast<VKAdapter>(adapter)->GetPhysicalDevice();
        std::vector<vk::SurfaceFormatKHR> vkFormats = pyDevice.getSurfaceFormatsKHR(m_surface);
        m_surfaceFormats.clear();
        for (const auto& vkFormat : vkFormats)
        {
            SurfaceFormat format;
            switch (vkFormat.format)
            {
            case vk::Format::eB8G8R8A8Unorm:
                format.format = Format::BGRA8_UNORM;
                break;
            case vk::Format::eR8G8B8A8Srgb:
                format.format = Format::RGBA8_SRGB;
                break;
            case vk::Format::eB8G8R8A8Srgb:
                format.format = Format::BGRA8_SRGB;
                break;
            case vk::Format::eR8G8B8A8Unorm:
                format.format = Format::RGBA8_UNORM;
                break;
            case vk::Format::eR16G16B16A16Sfloat:
                format.format = Format::RGBA16_FLOAT;
                break;
            case vk::Format::eA2B10G10R10UnormPack32:
                format.format = Format::RGB10A2_UNORM;
                break;
            case vk::Format::eR32G32B32A32Sfloat:
                format.format = Format::RGBA32_FLOAT;
                break;
            case vk::Format::eR8Unorm:
                format.format = Format::R8_UNORM;
                break;
            case vk::Format::eR16Sfloat:
                format.format = Format::R16_FLOAT;
                break;
            case vk::Format::eD32Sfloat:
                format.format = Format::D32F;
                break;
            case vk::Format::eD24UnormS8Uint:
                format.format = Format::D24S8;
                break;
            default:
                format.format = Format::UNDEFINED;
                break;
            }
            switch (vkFormat.colorSpace)
            {
            case vk::ColorSpaceKHR::eSrgbNonlinear:
                format.colorSpace = ColorSpace::SRGB_NONLINEAR;
                break;
            case vk::ColorSpaceKHR::eExtendedSrgbLinearEXT:
                format.colorSpace = ColorSpace::EXTENDED_SRGB_LINEAR;
                break;
            case vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT:
                format.colorSpace = ColorSpace::EXTENDED_SRGB_NONLINEAR;
                break;
            case vk::ColorSpaceKHR::eHdr10St2084EXT:
                format.colorSpace = ColorSpace::HDR10_ST2084;
                break;
            case vk::ColorSpaceKHR::eHdr10HlgEXT:
                format.colorSpace = ColorSpace::HDR10_HLG;
                break;
            case vk::ColorSpaceKHR::eDolbyvisionEXT:
                format.colorSpace = ColorSpace::DOLBY_VISION;
                break;
            case vk::ColorSpaceKHR::eAdobergbLinearEXT:
                format.colorSpace = ColorSpace::ADOBERGB_LINEAR;
                break;
            case vk::ColorSpaceKHR::eAdobergbNonlinearEXT:
                format.colorSpace = ColorSpace::ADOBERGB_NONLINEAR;
                break;
            case vk::ColorSpaceKHR::eDisplayP3NonlinearEXT:
                format.colorSpace = ColorSpace::DISPLAY_P3_NONLINEAR;
                break;
            case vk::ColorSpaceKHR::eDisplayP3LinearEXT:
                format.colorSpace = ColorSpace::DISPLAY_P3_LINEAR;
                break;
            case vk::ColorSpaceKHR::eDciP3NonlinearEXT:
                format.colorSpace = ColorSpace::DCI_P3_NONLINEAR;
                break;
            case vk::ColorSpaceKHR::eBt709LinearEXT:
                format.colorSpace = ColorSpace::BT709_LINEAR;
                break;
            case vk::ColorSpaceKHR::eBt709NonlinearEXT:
                format.colorSpace = ColorSpace::BT709_NONLINEAR;
                break;
            case vk::ColorSpaceKHR::eBt2020LinearEXT:
                format.colorSpace = ColorSpace::BT2020_LINEAR;
                break;
            case vk::ColorSpaceKHR::ePassThroughEXT:
                format.colorSpace = ColorSpace::PASS_THROUGH;
                break;
            default:
                format.colorSpace = ColorSpace::SRGB_NONLINEAR;
                break;
            }
            m_surfaceFormats.push_back(format);
        }
        return m_surfaceFormats;
    }
    std::vector<PresentMode> VKSurface::GetSupportedPresentModes(const Ref<Adapter>& adapter)
    {
        if (!adapter)
        {
            throw std::runtime_error("Adapter is null in VKSurface::GetCapabilities");
        }
        auto pyDevice = std::dynamic_pointer_cast<VKAdapter>(adapter)->GetPhysicalDevice();
        std::vector<vk::PresentModeKHR> vkPresentModes = pyDevice.getSurfacePresentModesKHR(m_surface);
        m_presentModes.clear();
        for (const auto& vkPresentMode : vkPresentModes)
        {
            switch (vkPresentMode)
            {
            case vk::PresentModeKHR::eImmediate:
                m_presentModes.push_back(PresentMode::Immediate);
                break;
            case vk::PresentModeKHR::eMailbox:
                m_presentModes.push_back(PresentMode::Mailbox);
                break;
            case vk::PresentModeKHR::eFifo:
                m_presentModes.push_back(PresentMode::Fifo);
                break;
            case vk::PresentModeKHR::eFifoRelaxed:
                m_presentModes.push_back(PresentMode::FifoRelaxed);
                break;
            default:
                break;
            }
        }
        return m_presentModes;
    }
    uint32_t VKSurface::GetPresentQueueFamilyIndex(const Ref<Adapter>& adapter) const
    {
        if (!adapter)
        {
            throw std::runtime_error("Adapter is null in VKSurface::GetPresentQueueFamilyIndex");
        }
        auto vkAdapter = std::dynamic_pointer_cast<VKAdapter>(adapter);
        vk::PhysicalDevice physicalDevice = vkAdapter->GetPhysicalDevice();
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queueFamilies.size(); i++)
        {
            vk::Bool32 presentSupport = physicalDevice.getSurfaceSupportKHR(i, m_surface);
            if (presentSupport)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find a present queue family index.");
    }
    Ref<Queue> VKSurface::GetPresentQueue(const Ref<Device>& device)
    {
        uint32_t presentQueueFamilyIndex = GetPresentQueueFamilyIndex(
            device->GetParentAdapter());
        vk::Queue vkQueue = std::dynamic_pointer_cast<VKDevice>(device)->GetHandle().getQueue(
            presentQueueFamilyIndex, 0);
        return VKQueue::Create(device, vkQueue, QueueType::Present, 0);
    }
} 
