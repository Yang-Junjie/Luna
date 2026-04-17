#ifndef LUNA_RHI_VKBUFFER_H
#define LUNA_RHI_VKBUFFER_H
#include "Buffer.h"
#include "vk_mem_alloc.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class Device;
}

namespace luna::RHI {
class LUNA_RHI_API VKBuffer final : public Buffer {
private:
    vk::Buffer m_buffer;
    Ref<Device> m_device;
    VmaAllocator m_allocator;
    VmaAllocation m_allocation;
    BufferCreateInfo m_createInfo;
    VmaAllocationInfo m_allocationInfo;
    friend class VKDevice;
    friend class VKCommandBufferEncoder;
    friend class VKDescriptorSet;

public:
    VKBuffer(const Ref<Device>& device, const VmaAllocator& allocator, const BufferCreateInfo& info);
    ~VKBuffer();
    static Ref<VKBuffer> Create(const Ref<Device>& device, const VmaAllocator& allocator, const BufferCreateInfo& info);
    uint64_t GetSize() const override;
    BufferUsageFlags GetUsage() const override;
    BufferMemoryUsage GetMemoryUsage() const override;
    void* Map() override;
    void Unmap() override;
    void Flush(uint64_t offset, uint64_t size) override;
    uint64_t GetDeviceAddress() const override;

    const vk::Buffer& GetHandle()
    {
        return m_buffer;
    }
};
}; // namespace luna::RHI
#endif
