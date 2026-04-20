#ifndef LUNA_RHI_D3D12DESCRIPTORPOOL_H
#define LUNA_RHI_D3D12DESCRIPTORPOOL_H
#include "D3D12Common.h"
#include "DescriptorPool.h"

namespace luna::RHI {
class LUNA_RHI_API D3D12DescriptorPool : public DescriptorPool {
private:
    ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
    Ref<Device> m_device;
    uint32_t m_cbvSrvUavOffset = 0;
    uint32_t m_samplerOffset = 0;
    uint32_t m_cbvSrvUavCapacity = 0;
    uint32_t m_samplerCapacity = 0;
    uint32_t m_cbvSrvUavDescriptorSize = 0;

public:
    D3D12DescriptorPool(const Ref<Device>& device, const DescriptorPoolCreateInfo& info);
    Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;
    void Reset() override;

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCBVSRVUAV();
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForCBVSRVUAV(uint32_t offset) const;

    ID3D12DescriptorHeap* GetCBVSRVUAVHeap() const
    {
        return m_cbvSrvUavHeap.Get();
    }

    uint32_t GetCBVSRVUAVDescriptorSize() const
    {
        return m_cbvSrvUavDescriptorSize;
    }
};
} // namespace luna::RHI

#endif
