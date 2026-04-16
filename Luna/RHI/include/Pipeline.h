#ifndef CACAO_CACAOPIPELINE_H
#define CACAO_CACAOPIPELINE_H
#include "PipelineDefs.h"
#include "ShaderModule.h"

#include <memory>
#include <vector>

namespace Cacao {
class PipelineLayout;
class PipelineCache;

class CACAO_API PipelineCache : public std::enable_shared_from_this<PipelineCache> {
public:
    virtual ~PipelineCache() = default;
    virtual std::vector<uint8_t> GetData() const = 0;
    virtual void Merge(std::span<const Ref<PipelineCache>> srcCaches) = 0;
};

struct MultisampleState {
    uint32_t RasterizationSamples = 1;
    bool SampleShadingEnable = false;
    float MinSampleShading = 0.0f;
    std::vector<uint32_t> SampleMask;
    bool AlphaToCoverageEnable = false;
    bool AlphaToOneEnable = false;
};

struct GraphicsPipelineCreateInfo {
    std::vector<Ref<ShaderModule>> Shaders;
    std::vector<VertexInputBinding> VertexBindings;
    std::vector<VertexInputAttribute> VertexAttributes;
    InputAssemblyState InputAssembly;
    RasterizationState Rasterizer;
    DepthStencilState DepthStencil;
    ColorBlendState ColorBlend;
    MultisampleState Multisample;
    std::vector<Format> ColorAttachmentFormats;
    Format DepthStencilFormat = Format::UNDEFINED;
    Ref<PipelineLayout> Layout;
    Ref<PipelineCache> Cache = nullptr;
};

class CACAO_API GraphicsPipeline : public std::enable_shared_from_this<GraphicsPipeline> {
public:
    virtual ~GraphicsPipeline() = default;
    virtual Ref<PipelineLayout> GetLayout() const = 0;
};

struct ComputePipelineCreateInfo {
    Ref<ShaderModule> ComputeShader;
    Ref<PipelineLayout> Layout;
    Ref<PipelineCache> Cache = nullptr;
};

class CACAO_API ComputePipeline : public std::enable_shared_from_this<ComputePipeline> {
public:
    virtual ~ComputePipeline() = default;
    virtual Ref<PipelineLayout> GetLayout() const = 0;
};

struct RayTracingPipelineCreateInfo {
    std::vector<Ref<ShaderModule>> Shaders;
    uint32_t MaxRecursionDepth = 1;
    Ref<PipelineLayout> Layout;
    Ref<PipelineCache> Cache = nullptr;
};

class CACAO_API RayTracingPipeline : public std::enable_shared_from_this<RayTracingPipeline> {
public:
    virtual ~RayTracingPipeline() = default;
    virtual Ref<PipelineLayout> GetLayout() const = 0;
};
} // namespace Cacao
#endif
