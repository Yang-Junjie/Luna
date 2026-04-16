#ifndef CACAO_D3D11BUFFER_H
#define CACAO_D3D11BUFFER_H
#include "D3D11Common.h"
#include <Buffer.h>

namespace Cacao
{
    class D3D11Device;

    class CACAO_API D3D11Buffer : public Buffer
    {
    public:
        D3D11Buffer(Ref<D3D11Device> device, const BufferCreateInfo& createInfo);

        uint64_t GetSize() const override { return m_size; }
        BufferUsageFlags GetUsage() const override { return m_usage; }
        BufferMemoryUsage GetMemoryUsage() const override { return m_memoryUsage; }
        void* Map() override;
        void Unmap() override;
        void Flush(uint64_t offset, uint64_t size) override;
        uint64_t GetDeviceAddress() const override { return 0; }

        ID3D11Buffer* GetNativeBuffer() const { return m_buffer.Get(); }
        void EnsureStructuredSRV(uint32_t stride);
        ComPtr<ID3D11ShaderResourceView> GetStructuredSRV() const { return m_structuredSRV; }

    private:
        Ref<D3D11Device> m_device;
        ComPtr<ID3D11Buffer> m_buffer;
        uint64_t m_size = 0;
        BufferUsageFlags m_usage;
        BufferMemoryUsage m_memoryUsage;
        D3D11_MAPPED_SUBRESOURCE m_mappedData{};
        bool m_isMapped = false;
        bool m_needsStagingForMap = false;
        std::vector<uint8_t> m_cpuStagingData;
        ComPtr<ID3D11Buffer> m_structuredBuffer;
        ComPtr<ID3D11ShaderResourceView> m_structuredSRV;
        uint32_t m_structuredStride = 0;
    };
}
#endif
