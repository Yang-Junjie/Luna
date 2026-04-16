#ifndef CACAO_D3D12BUFFER_H
#define CACAO_D3D12BUFFER_H
#include "D3D12Common.h"
#include "Buffer.h"
#include "D3D12MemAlloc.h"

namespace Cacao
{
    class CACAO_API D3D12Buffer final : public Buffer
    {
    private:
        ComPtr<ID3D12Resource> m_resource;
        D3D12MA::Allocation* m_allocation = nullptr;
        Ref<Device> m_device;
        BufferCreateInfo m_createInfo;
        void* m_mappedData = nullptr;

        friend class D3D12Device;
        friend class D3D12CommandBufferEncoder;
        friend class D3D12DescriptorSet;
        friend class D3D12AccelerationStructure;
        friend class D3D12ShaderBindingTable;
        ID3D12Resource* GetHandle() const { return m_resource.Get(); }

    public:
        D3D12Buffer(const Ref<Device>& device, const BufferCreateInfo& info);
        ~D3D12Buffer() override;
        static Ref<D3D12Buffer> Create(const Ref<Device>& device, const BufferCreateInfo& info);

        uint64_t GetSize() const override;
        BufferUsageFlags GetUsage() const override;
        BufferMemoryUsage GetMemoryUsage() const override;
        void* Map() override;
        void Unmap() override;
        void Flush(uint64_t offset, uint64_t size) override;
        uint64_t GetDeviceAddress() const override;
    };
}

#endif
