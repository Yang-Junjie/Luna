#ifndef CACAO_VKSTAGINGBUFFER_H
#define CACAO_VKSTAGINGBUFFER_H

#include "StagingBuffer.h"
#include "Impls/Vulkan/VKBuffer.h"
#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

namespace Cacao
{
    class VKDevice;

    struct VKStagingBlock
    {
        Ref<VKBuffer> buffer;
        void* mappedPtr = nullptr;
        uint64_t size = 0;
        uint64_t offset = 0;
    };

    class CACAO_API VKStagingBufferPool final : public StagingBufferPool
    {
    private:
        Ref<VKDevice> m_device;
        uint64_t m_blockSize;
        uint32_t m_maxFramesInFlight;
        uint32_t m_currentFrame = 0;

        std::vector<std::vector<VKStagingBlock>> m_frameBlocks;
        uint64_t m_totalAllocated = 0;

        VKStagingBlock& GetOrCreateBlock(uint64_t requiredSize);

    public:
        VKStagingBufferPool(const Ref<Device>& device, uint64_t blockSize, uint32_t maxFramesInFlight);
        ~VKStagingBufferPool() override = default;

        StagingAllocation Allocate(uint64_t size, uint64_t alignment = 256) override;
        void Reset() override;
        void AdvanceFrame() override;

        uint64_t GetTotalAllocated() const override { return m_totalAllocated; }
        uint64_t GetCapacity() const override { return m_blockSize * m_frameBlocks.size(); }
    };
}

#endif
