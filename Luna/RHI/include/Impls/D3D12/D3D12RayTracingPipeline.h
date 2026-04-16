#ifndef CACAO_D3D12RAYTRACINGPIPELINE_H
#define CACAO_D3D12RAYTRACINGPIPELINE_H
#include "D3D12Common.h"
#include "Pipeline.h"

namespace Cacao
{
    class CACAO_API D3D12RayTracingPipeline final : public RayTracingPipeline
    {
    public:
        D3D12RayTracingPipeline(const Ref<Device>& device, const RayTracingPipelineCreateInfo& info);

        Ref<PipelineLayout> GetLayout() const override { return m_layout; }
        ID3D12StateObject* GetStateObject() const { return m_stateObject.Get(); }

    private:
        ComPtr<ID3D12StateObject> m_stateObject;
        Ref<PipelineLayout> m_layout;
    };
}

#endif
