#ifndef LUNA_RHI_D3D12PIPELINELAYOUT_H
#define LUNA_RHI_D3D12PIPELINELAYOUT_H
#include "D3D12Common.h"
#include "PipelineLayout.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace luna::RHI {
class LUNA_RHI_API D3D12PipelineLayout : public PipelineLayout {
public:
    static constexpr uint32_t InvalidRootParameterIndex = (std::numeric_limits<uint32_t>::max)();

    struct DescriptorSetRootParameters {
        uint32_t cbvSrvUavRootIndex{InvalidRootParameterIndex};
        uint32_t samplerRootIndex{InvalidRootParameterIndex};
    };

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    Ref<Device> m_device;
    PipelineLayoutCreateInfo m_createInfo;
    std::vector<DescriptorSetRootParameters> m_setRootParameters;

    friend class D3D12GraphicsPipeline;
    friend class D3D12ComputePipeline;
    friend class D3D12CommandBufferEncoder;
    friend class D3D12RayTracingPipeline;

public:
    D3D12PipelineLayout(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);
    ~D3D12PipelineLayout() = default;
    static Ref<D3D12PipelineLayout> Create(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);

    ID3D12RootSignature* GetHandle() const
    {
        return m_rootSignature.Get();
    }

    const PipelineLayoutCreateInfo& GetCreateInfo() const
    {
        return m_createInfo;
    }

    const DescriptorSetRootParameters* GetDescriptorSetRootParameters(uint32_t setIndex) const noexcept
    {
        if (setIndex >= m_setRootParameters.size()) {
            return nullptr;
        }
        return &m_setRootParameters[setIndex];
    }
};
} // namespace luna::RHI

#endif
