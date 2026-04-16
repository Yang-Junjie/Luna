#ifndef CACAO_WGPU_BUFFER_H
#define CACAO_WGPU_BUFFER_H

#include "WGPUCommon.h"
#include "Buffer.h"

namespace Cacao
{
    class WGPUDevice;

    class CACAO_API WGPUBufferImpl final : public Buffer
    {
    private:
        ::WGPUBuffer m_buffer = nullptr;
        ::WGPUDevice m_wgpuDevice = nullptr;
        BufferCreateInfo m_createInfo;
        void* m_mappedPtr = nullptr;
        bool m_persistentlyMapped = false;

        friend class WGPUDevice;
        friend class WGPUCommandBufferEncoder;
        friend class WGPUDescriptorSet;

    public:
        WGPUBufferImpl(::WGPUDevice device, const BufferCreateInfo& info);
        ~WGPUBufferImpl() override;

        uint64_t GetSize() const override;
        BufferUsageFlags GetUsage() const override;
        BufferMemoryUsage GetMemoryUsage() const override;
        void* Map() override;
        void Unmap() override;
        void Flush(uint64_t offset = 0, uint64_t size = UINT64_MAX) override;
        uint64_t GetDeviceAddress() const override;

        ::WGPUBuffer GetNativeBuffer() const { return m_buffer; }
    };
}

#endif
