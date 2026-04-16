#ifndef CACAO_STAGINGBUFFER_H
#define CACAO_STAGINGBUFFER_H

#include "Core.h"

#include <cstdint>

#include <vector>

namespace Cacao {
class Buffer;
class Device;

struct StagingAllocation {
    Ref<Buffer> buffer;
    uint64_t offset = 0;
    void* mappedPtr = nullptr;
    uint64_t size = 0;
};

class CACAO_API StagingBufferPool {
public:
    virtual ~StagingBufferPool() = default;

    virtual StagingAllocation Allocate(uint64_t size, uint64_t alignment = 256) = 0;

    virtual void Reset() = 0;

    virtual void AdvanceFrame() = 0;

    virtual uint64_t GetTotalAllocated() const = 0;
    virtual uint64_t GetCapacity() const = 0;

    static Ref<StagingBufferPool>
        Create(const Ref<Device>& device, uint64_t blockSize = 64 * 1'024 * 1'024, uint32_t maxFramesInFlight = 2);
};
} // namespace Cacao

#endif
