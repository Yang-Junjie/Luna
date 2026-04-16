#ifndef CACAO_WGPU_PIPELINE_H
#define CACAO_WGPU_PIPELINE_H

#include "Pipeline.h"

#include <webgpu/webgpu.h>

namespace Cacao {
class WGPUDevice;

class CACAO_API WGPUGraphicsPipeline final : public GraphicsPipeline {
private:
    Ref<WGPUDevice> m_device;
    Ref<PipelineLayout> m_layout;
    GraphicsPipelineCreateInfo m_createInfo;
    ::WGPURenderPipeline m_pipeline = nullptr;

public:
    WGPUGraphicsPipeline(const Ref<Device>& device, const GraphicsPipelineCreateInfo& info);
    ~WGPUGraphicsPipeline() override;

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }

    ::WGPURenderPipeline GetHandle() const
    {
        return m_pipeline;
    }
};

class CACAO_API WGPUComputePipelineImpl final : public ComputePipeline {
private:
    Ref<WGPUDevice> m_device;
    Ref<PipelineLayout> m_layout;
    ComputePipelineCreateInfo m_createInfo;
    ::WGPUComputePipeline m_pipeline = nullptr;

public:
    WGPUComputePipelineImpl(const Ref<Device>& device, const ComputePipelineCreateInfo& info);
    ~WGPUComputePipelineImpl() override;

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }

    ::WGPUComputePipeline GetHandle() const
    {
        return m_pipeline;
    }
};
} // namespace Cacao

#endif
