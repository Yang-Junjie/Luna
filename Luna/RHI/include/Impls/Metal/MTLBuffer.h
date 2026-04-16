#ifndef CACAO_MTLBUFFER_H
#define CACAO_MTLBUFFER_H
#ifdef __APPLE__
#include "Buffer.h"
#include "MTLCommon.h"

namespace Cacao {
class CACAO_API MTLBufferImpl final : public Buffer {
public:
    MTLBufferImpl(id device, const BufferCreateInfo& info);
    ~MTLBufferImpl() override;

    void* Map() override;
    void Unmap() override;
    void Flush(uint64_t offset = 0, uint64_t size = UINT64_MAX) override;

    uint64_t GetSize() const override
    {
        return m_createInfo.Size;
    }

    BufferUsageFlags GetUsage() const override
    {
        return m_createInfo.Usage;
    }

    BufferMemoryUsage GetMemoryUsage() const override
    {
        return m_createInfo.MemoryUsage;
    }

    uint64_t GetDeviceAddress() const override
    {
        return 0;
    }

    id GetHandle() const
    {
        return m_buffer;
    }

private:
    BufferCreateInfo m_createInfo;
    id m_buffer = nullptr; // id<MTLBuffer>
};
} // namespace Cacao
#endif // __APPLE__
#endif
