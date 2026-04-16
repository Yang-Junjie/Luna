#ifndef CACAO_D3D12SAMPLER_H
#define CACAO_D3D12SAMPLER_H
#include "D3D12Common.h"
#include "Sampler.h"

namespace Cacao {
class CACAO_API D3D12Sampler : public Sampler {
private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle = {};
    ComPtr<ID3D12DescriptorHeap> m_stagingHeap;
    Ref<Device> m_device;
    SamplerCreateInfo m_createInfo;

    friend class D3D12DescriptorSet;

public:
    D3D12Sampler(const Ref<Device>& device, const SamplerCreateInfo& createInfo);
    static Ref<D3D12Sampler> Create(const Ref<Device>& device, const SamplerCreateInfo& createInfo);
    const SamplerCreateInfo& GetInfo() const override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const
    {
        return m_cpuHandle;
    }
};
} // namespace Cacao

#endif
