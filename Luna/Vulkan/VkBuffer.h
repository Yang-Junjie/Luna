#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace luna::vkcore {

class VMAAllocator;

class Buffer {
public:
    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    bool isValid() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

    vk::Buffer get() const
    {
        return m_handle;
    }

    VmaAllocation getAllocation() const
    {
        return m_allocation;
    }

    const VmaAllocationInfo& getAllocationInfo() const
    {
        return m_info;
    }

    void* getMappedData() const
    {
        return m_info.pMappedData;
    }

    void reset();

    operator vk::Buffer() const
    {
        return m_handle;
    }

    bool operator==(const Buffer& other) const
    {
        return m_handle == other.m_handle;
    }

private:
    friend class VMAAllocator;

    void assign(vk::Buffer handle, VmaAllocation allocation, const VmaAllocationInfo& info, VmaAllocator allocator);

    vk::Buffer m_handle{};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VmaAllocationInfo m_info{};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
};

} // namespace luna::vkcore
