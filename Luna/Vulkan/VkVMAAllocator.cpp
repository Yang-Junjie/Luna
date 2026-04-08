#include "VkVMAAllocator.h"

#include <cmath>
#include <cstdlib>

#include <Core/Log.h>
#include <utility>

namespace luna::vkcore {

VMAAllocator::~VMAAllocator()
{
    destroy();
}

VMAAllocator::VMAAllocator(VMAAllocator&& other) noexcept
{
    *this = std::move(other);
}

VMAAllocator& VMAAllocator::operator=(VMAAllocator&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    destroy();

    m_allocator = other.m_allocator;
    m_device = other.m_device;

    other.m_allocator = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    return *this;
}

bool VMAAllocator::create(vk::Instance instance,
                          vk::PhysicalDevice physical_device,
                          vk::Device device,
                          VmaAllocatorCreateFlags flags)
{
    destroy();

    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.instance = instance;
    allocator_info.flags = flags;

    const VkResult result = vmaCreateAllocator(&allocator_info, &m_allocator);
    if (result != VK_SUCCESS) {
        return false;
    }

    m_device = device;
    return true;
}

void VMAAllocator::destroy()
{
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
    }

    m_allocator = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

Buffer VMAAllocator::createBuffer(size_t size,
                                  vk::BufferUsageFlags usage,
                                  VmaMemoryUsage memory_usage,
                                  VmaAllocationCreateFlags allocation_flags) const
{
    Buffer buffer;
    if (m_allocator == VK_NULL_HANDLE) {
        return buffer;
    }

    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = size;
    buffer_info.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo allocation_info = {};
    allocation_info.usage = memory_usage;
    allocation_info.flags = allocation_flags;

    VkBuffer raw_buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_details{};
    const VkResult create_result =
        vmaCreateBuffer(m_allocator, &buffer_info, &allocation_info, &raw_buffer, &allocation, &allocation_details);
    if (create_result != VK_SUCCESS) {
        LUNA_CORE_FATAL("Failed to create VMA buffer: {}", vk::to_string(static_cast<vk::Result>(create_result)));
        std::abort();
    }
    buffer.assign(raw_buffer, allocation, allocation_details, m_allocator);
    return buffer;
}

Image VMAAllocator::createImage(vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped) const
{
    Image image;
    if (m_allocator == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE) {
        return image;
    }

    vk::ImageCreateInfo image_info = {};
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = format;
    image_info.extent = size;
    image_info.mipLevels =
        mipmapped ? static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1 : 1;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.usage = usage;

    VmaAllocationCreateInfo allocation_info = {};
    allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocation_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImage raw_image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    const VkResult create_result = vmaCreateImage(m_allocator,
                                                  reinterpret_cast<const VkImageCreateInfo*>(&image_info),
                                                  &allocation_info,
                                                  &raw_image,
                                                  &allocation,
                                                  nullptr);
    if (create_result != VK_SUCCESS) {
        LUNA_CORE_FATAL("Failed to create VMA image: {}", vk::to_string(static_cast<vk::Result>(create_result)));
        std::abort();
    }

    image.assign(raw_image, allocation, m_allocator, size, format, image_info.mipLevels);

    vk::ImageAspectFlags aspect_flags = vk::ImageAspectFlagBits::eColor;
    if (format == vk::Format::eD32Sfloat) {
        aspect_flags = vk::ImageAspectFlagBits::eDepth;
    }

    if (!image.getViewObject().create(m_device, image.get(), format, aspect_flags, image_info.mipLevels)) {
        LUNA_CORE_FATAL("Failed to create image view for format {}", vk::to_string(format));
        std::abort();
    }
    return image;
}

} // namespace luna::vkcore
