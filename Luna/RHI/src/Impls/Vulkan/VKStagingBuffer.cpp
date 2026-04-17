#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKStagingBuffer.h"

#include <algorithm>

namespace Cacao {
Ref<StagingBufferPool>
    StagingBufferPool::Create(const Ref<Device>& device, uint64_t blockSize, uint32_t maxFramesInFlight)
{
    return std::make_shared<VKStagingBufferPool>(device, blockSize, maxFramesInFlight);
}

VKStagingBufferPool::VKStagingBufferPool(const Ref<Device>& device, uint64_t blockSize, uint32_t maxFramesInFlight)
    : m_device(std::dynamic_pointer_cast<VKDevice>(device)),
      m_blockSize(blockSize),
      m_maxFramesInFlight(maxFramesInFlight)
{
    m_frameBlocks.resize(maxFramesInFlight);
}

VKStagingBlock& VKStagingBufferPool::GetOrCreateBlock(uint64_t requiredSize)
{
    auto& blocks = m_frameBlocks[m_currentFrame];

    if (!blocks.empty()) {
        auto& last = blocks.back();
        if (last.offset + requiredSize <= last.size) {
            return last;
        }
    }

    uint64_t allocSize = (std::max)(m_blockSize, requiredSize);

    BufferCreateInfo bufferInfo = {};
    bufferInfo.Size = allocSize;
    bufferInfo.Usage = BufferUsageFlags::TransferSrc;
    bufferInfo.MemoryUsage = BufferMemoryUsage::CpuToGpu;
    bufferInfo.Name = "StagingBlock";

    auto buffer = std::dynamic_pointer_cast<VKBuffer>(m_device->CreateBuffer(bufferInfo));

    VKStagingBlock block;
    block.buffer = buffer;
    block.mappedPtr = buffer->Map();
    block.size = allocSize;
    block.offset = 0;

    blocks.push_back(std::move(block));
    return blocks.back();
}

StagingAllocation VKStagingBufferPool::Allocate(uint64_t size, uint64_t alignment)
{
    auto& block = GetOrCreateBlock(size);

    uint64_t alignedOffset = (block.offset + alignment - 1) & ~(alignment - 1);
    if (alignedOffset + size > block.size) {
        auto& newBlock = GetOrCreateBlock(size);
        alignedOffset = 0;
        StagingAllocation alloc;
        alloc.buffer = newBlock.buffer;
        alloc.offset = 0;
        alloc.mappedPtr = newBlock.mappedPtr;
        alloc.size = size;
        newBlock.offset = size;
        m_totalAllocated += size;
        return alloc;
    }

    StagingAllocation alloc;
    alloc.buffer = block.buffer;
    alloc.offset = alignedOffset;
    alloc.mappedPtr = static_cast<uint8_t*>(block.mappedPtr) + alignedOffset;
    alloc.size = size;
    block.offset = alignedOffset + size;
    m_totalAllocated += size;
    return alloc;
}

void VKStagingBufferPool::Reset()
{
    for (auto& blocks : m_frameBlocks) {
        for (auto& block : blocks) {
            block.offset = 0;
        }
    }
    m_totalAllocated = 0;
}

void VKStagingBufferPool::AdvanceFrame()
{
    m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;
    auto& blocks = m_frameBlocks[m_currentFrame];
    for (auto& block : blocks) {
        block.offset = 0;
    }
}
} // namespace Cacao
