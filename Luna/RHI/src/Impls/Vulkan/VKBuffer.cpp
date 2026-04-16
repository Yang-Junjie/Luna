#include "Impls/Vulkan/VKBuffer.h"
#include "Impls/Vulkan/VKDevice.h"
Cacao::VKBuffer::VKBuffer(const Ref<Device>& device, const VmaAllocator& allocator, const BufferCreateInfo& info) :
    m_device(device), m_allocator(allocator), m_createInfo(info)
{
    if (!m_device)
    {
        throw std::runtime_error("VKBuffer created with null device");
    }
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = info.Size;
    vk::BufferUsageFlags usageFlags;
    if (info.Usage & BufferUsageFlags::VertexBuffer)
        usageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
    if (info.Usage & BufferUsageFlags::IndexBuffer)
        usageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
    if (info.Usage & BufferUsageFlags::UniformBuffer)
        usageFlags |= vk::BufferUsageFlagBits::eUniformBuffer;
    if (info.Usage & BufferUsageFlags::StorageBuffer)
        usageFlags |= vk::BufferUsageFlagBits::eStorageBuffer;
    if (info.Usage & BufferUsageFlags::TransferSrc)
        usageFlags |= vk::BufferUsageFlagBits::eTransferSrc;
    if (info.Usage & BufferUsageFlags::TransferDst)
        usageFlags |= vk::BufferUsageFlagBits::eTransferDst;
    if (info.Usage & BufferUsageFlags::IndirectBuffer)
        usageFlags |= vk::BufferUsageFlagBits::eIndirectBuffer;
    if (info.Usage & BufferUsageFlags::ShaderDeviceAddress)
        usageFlags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
    if (info.Usage & BufferUsageFlags::AccelerationStructure)
    {
        usageFlags |= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR;
        usageFlags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
    }
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    VmaAllocationCreateInfo allocInfo{};
    switch (info.MemoryUsage)
    {
    case BufferMemoryUsage::GpuOnly:
        {
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
        }
    case BufferMemoryUsage::CpuOnly:
        {
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
            break;
        }
    case BufferMemoryUsage::CpuToGpu:
        {
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            break;
        }
    case BufferMemoryUsage::GpuToCpu:
        {
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            break;
        }
    default:
        {
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
        }
    }
    vmaCreateBuffer(m_allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo,
                    reinterpret_cast<VkBuffer*>(&m_buffer), &m_allocation,
                    &m_allocationInfo);
    if (!m_buffer)
    {
        throw std::runtime_error("Failed to create Vulkan buffer");
    }
}
Cacao::Ref<Cacao::VKBuffer> Cacao::VKBuffer::Create(const Ref<Device>& device, const VmaAllocator& allocator,
                                                    const BufferCreateInfo& info)
{
    return CreateRef<VKBuffer>(device, allocator, info);
}
uint64_t Cacao::VKBuffer::GetSize() const
{
    return m_createInfo.Size;
}
Cacao::BufferUsageFlags Cacao::VKBuffer::GetUsage() const
{
    return m_createInfo.Usage;
}
Cacao::BufferMemoryUsage Cacao::VKBuffer::GetMemoryUsage() const
{
    return m_createInfo.MemoryUsage;
}
void* Cacao::VKBuffer::Map()
{
    void* mapped;
    vmaMapMemory(m_allocator, m_allocation, &mapped);
    return mapped;
}
void Cacao::VKBuffer::Unmap()
{
    vmaUnmapMemory(m_allocator, m_allocation);
}
void Cacao::VKBuffer::Flush(uint64_t offset, uint64_t size)
{
    vmaFlushAllocation(m_allocator, m_allocation, offset, size);
}
Cacao::VKBuffer::~VKBuffer()
{
    if (m_allocator && m_allocation)
    {
        vmaDestroyBuffer(m_allocator, static_cast<VkBuffer>(m_buffer), m_allocation);
        m_buffer = nullptr;
        m_allocation = nullptr;
    }
}

uint64_t Cacao::VKBuffer::GetDeviceAddress() const
{
    vk::BufferDeviceAddressInfo addressInfo = vk::BufferDeviceAddressInfo().setBuffer(m_buffer);
    return std::dynamic_pointer_cast<VKDevice>(m_device)->GetHandle().getBufferAddress(addressInfo);
}
