#pragma once

#include "VkBuffer.h"
#include "VkImage.h"

#include <cstddef>

namespace luna::vkcore {

class VMAAllocator {
public:
    VMAAllocator() = default;
    ~VMAAllocator();

    VMAAllocator(const VMAAllocator&) = delete;
    VMAAllocator& operator=(const VMAAllocator&) = delete;

    VMAAllocator(VMAAllocator&& other) noexcept;
    VMAAllocator& operator=(VMAAllocator&& other) noexcept;

    bool create(vk::Instance instance,
                vk::PhysicalDevice physical_device,
                vk::Device device,
                VmaAllocatorCreateFlags flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
    void destroy();

    bool isValid() const
    {
        return m_allocator != VK_NULL_HANDLE;
    }

    VmaAllocator get() const
    {
        return m_allocator;
    }

    vk::Device getDevice() const
    {
        return m_device;
    }

    Buffer createBuffer(size_t size,
                        vk::BufferUsageFlags usage,
                        VmaMemoryUsage memory_usage,
                        VmaAllocationCreateFlags allocation_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT) const;
    Image createImage(vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false) const;

private:
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    vk::Device m_device{};
};

} // namespace luna::vkcore
