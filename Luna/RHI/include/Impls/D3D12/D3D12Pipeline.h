#ifndef LUNA_RHI_D3D12PIPELINE_H
#define LUNA_RHI_D3D12PIPELINE_H
#include "D3D12Common.h"
#include "Pipeline.h"

namespace luna::RHI {
class LUNA_RHI_API D3D12GraphicsPipeline final : public GraphicsPipeline {
private:
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    Ref<Device> m_device;
    Ref<PipelineLayout> m_layout;
    D3D12_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    std::vector<VertexInputBinding> m_vertexBindings;

    friend class D3D12CommandBufferEncoder;

    ID3D12PipelineState* GetHandle() const
    {
        return m_pipelineState.Get();
    }

    ID3D12RootSignature* GetRootSignature() const
    {
        return m_rootSignature.Get();
    }

    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const
    {
        return m_topology;
    }

    uint32_t GetVertexStride(uint32_t binding) const
    {
        for (auto& b : m_vertexBindings) {
            if (b.Binding == binding) {
                return b.Stride;
            }
        }
        return 0;
    }

public:
    D3D12GraphicsPipeline(const Ref<Device>& device, const GraphicsPipelineCreateInfo& info);

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }
};

class LUNA_RHI_API D3D12ComputePipeline final : public ComputePipeline {
private:
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    Ref<Device> m_device;
    Ref<PipelineLayout> m_layout;

    friend class D3D12CommandBufferEncoder;

    ID3D12PipelineState* GetHandle() const
    {
        return m_pipelineState.Get();
    }

    ID3D12RootSignature* GetRootSignature() const
    {
        return m_rootSignature.Get();
    }

public:
    D3D12ComputePipeline(const Ref<Device>& device, const ComputePipelineCreateInfo& info);

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }
};
} // namespace luna::RHI

#endif
