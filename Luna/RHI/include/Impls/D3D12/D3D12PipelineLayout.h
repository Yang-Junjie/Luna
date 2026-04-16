#ifndef CACAO_D3D12PIPELINELAYOUT_H
#define CACAO_D3D12PIPELINELAYOUT_H
#include "D3D12Common.h"
#include "PipelineLayout.h"

namespace Cacao
{
    class CACAO_API D3D12PipelineLayout : public PipelineLayout
    {
    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        Ref<Device> m_device;
        PipelineLayoutCreateInfo m_createInfo;

        friend class D3D12GraphicsPipeline;
        friend class D3D12ComputePipeline;
        friend class D3D12CommandBufferEncoder;
        friend class D3D12RayTracingPipeline;

    public:
        D3D12PipelineLayout(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);
        ~D3D12PipelineLayout() = default;
        static Ref<D3D12PipelineLayout> Create(const Ref<Device>& device, const PipelineLayoutCreateInfo& info);
        ID3D12RootSignature* GetHandle() const { return m_rootSignature.Get(); }
        const PipelineLayoutCreateInfo& GetCreateInfo() const { return m_createInfo; }
    };
}

#endif
