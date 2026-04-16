#ifndef CACAO_D3D12DESCRIPTORPOOL_H
#define CACAO_D3D12DESCRIPTORPOOL_H
#include "D3D12Common.h"
#include "DescriptorPool.h"

namespace Cacao
{
    class CACAO_API D3D12DescriptorPool : public DescriptorPool
    {
    private:
        ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
        ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
        Ref<Device> m_device;
        uint32_t m_cbvSrvUavOffset = 0;
        uint32_t m_samplerOffset = 0;
        uint32_t m_cbvSrvUavDescriptorSize = 0;
        uint32_t m_samplerDescriptorSize = 0;

    public:
        D3D12DescriptorPool(const Ref<Device>& device, const DescriptorPoolCreateInfo& info);
        Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;
        void Reset() override;

        D3D12_CPU_DESCRIPTOR_HANDLE AllocateCBVSRVUAV();
        D3D12_CPU_DESCRIPTOR_HANDLE AllocateSampler();
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForCBVSRVUAV(uint32_t offset) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForSampler(uint32_t offset) const;
        ID3D12DescriptorHeap* GetCBVSRVUAVHeap() const { return m_cbvSrvUavHeap.Get(); }
        ID3D12DescriptorHeap* GetSamplerHeap() const { return m_samplerHeap.Get(); }
        uint32_t GetCBVSRVUAVDescriptorSize() const { return m_cbvSrvUavDescriptorSize; }
        uint32_t GetSamplerDescriptorSize() const { return m_samplerDescriptorSize; }
    };
}

#endif
