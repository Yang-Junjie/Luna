#ifndef CACAO_MTLPIPELINE_H
#define CACAO_MTLPIPELINE_H
#ifdef __APPLE__
#include "MTLCommon.h"
#include "Pipeline.h"

namespace Cacao
{
    class CACAO_API MTLGraphicsPipeline final : public GraphicsPipeline
    {
    public:
        MTLGraphicsPipeline(const Ref<Device>& device, const GraphicsPipelineCreateInfo& info);
        ~MTLGraphicsPipeline() override = default;
        Ref<PipelineLayout> GetLayout() const override { return m_layout; }
        id GetPipelineState() const { return m_pipelineState; }

    private:
        Ref<PipelineLayout> m_layout;
        id m_pipelineState = nullptr; // id<MTLRenderPipelineState>
    };

    class CACAO_API MTLComputePipeline final : public ComputePipeline
    {
    public:
        MTLComputePipeline(const Ref<Device>& device, const ComputePipelineCreateInfo& info);
        ~MTLComputePipeline() override = default;
        Ref<PipelineLayout> GetLayout() const override { return m_layout; }
        id GetPipelineState() const { return m_pipelineState; }

    private:
        Ref<PipelineLayout> m_layout;
        id m_pipelineState = nullptr; // id<MTLComputePipelineState>
    };
}
#endif // __APPLE__
#endif
