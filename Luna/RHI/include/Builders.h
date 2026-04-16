#ifndef CACAO_BUILDERS_H
#define CACAO_BUILDERS_H
#include "Buffer.h"
#include "DescriptorPool.h"
#include "DescriptorSetLayout.h"
#include "Pipeline.h"
#include "PipelineLayout.h"
#include "Sampler.h"
#include "Swapchain.h"
#include "Texture.h"

namespace Cacao {
class BufferBuilder {
    BufferCreateInfo m_info;

public:
    BufferBuilder& SetSize(uint64_t size)
    {
        m_info.Size = size;
        return *this;
    }

    BufferBuilder& SetUsage(BufferUsageFlags usage)
    {
        m_info.Usage = usage;
        return *this;
    }

    BufferBuilder& AddUsage(BufferUsageFlags usage)
    {
        m_info.Usage = m_info.Usage | usage;
        return *this;
    }

    BufferBuilder& SetMemoryUsage(BufferMemoryUsage usage)
    {
        m_info.MemoryUsage = usage;
        return *this;
    }

    BufferBuilder& SetName(const std::string& name)
    {
        m_info.Name = name;
        return *this;
    }

    BufferBuilder& SetInitialData(const void* data)
    {
        m_info.InitialData = data;
        return *this;
    }

    const BufferCreateInfo& Build() const
    {
        return m_info;
    }

    operator const BufferCreateInfo&() const
    {
        return m_info;
    }
};

class TextureBuilder {
    TextureCreateInfo m_info;

public:
    TextureBuilder& SetType(TextureType type)
    {
        m_info.Type = type;
        return *this;
    }

    TextureBuilder& SetWidth(uint32_t w)
    {
        m_info.Width = w;
        return *this;
    }

    TextureBuilder& SetHeight(uint32_t h)
    {
        m_info.Height = h;
        return *this;
    }

    TextureBuilder& SetDepth(uint32_t d)
    {
        m_info.Depth = d;
        return *this;
    }

    TextureBuilder& SetSize(uint32_t w, uint32_t h, uint32_t d = 1)
    {
        m_info.Width = w;
        m_info.Height = h;
        m_info.Depth = d;
        return *this;
    }

    TextureBuilder& SetArrayLayers(uint32_t layers)
    {
        m_info.ArrayLayers = layers;
        return *this;
    }

    TextureBuilder& SetMipLevels(uint32_t levels)
    {
        m_info.MipLevels = levels;
        return *this;
    }

    TextureBuilder& SetFormat(Format fmt)
    {
        m_info.Format = fmt;
        return *this;
    }

    TextureBuilder& SetUsage(TextureUsageFlags usage)
    {
        m_info.Usage = usage;
        return *this;
    }

    TextureBuilder& AddUsage(TextureUsageFlags usage)
    {
        m_info.Usage = m_info.Usage | usage;
        return *this;
    }

    TextureBuilder& SetInitialLayout(ResourceState state)
    {
        m_info.InitialState = state;
        return *this;
    }

    TextureBuilder& SetInitialState(ResourceState state)
    {
        m_info.InitialState = state;
        return *this;
    }

    TextureBuilder& SetSampleCount(SampleCount count)
    {
        m_info.SampleCount = count;
        return *this;
    }

    TextureBuilder& SetName(const std::string& name)
    {
        m_info.Name = name;
        return *this;
    }

    TextureBuilder& SetInitialData(void* data)
    {
        m_info.InitialData = data;
        return *this;
    }

    const TextureCreateInfo& Build() const
    {
        return m_info;
    }

    operator const TextureCreateInfo&() const
    {
        return m_info;
    }
};

class SamplerBuilder {
    SamplerCreateInfo m_info;

public:
    SamplerBuilder& SetMagFilter(Filter f)
    {
        m_info.MagFilter = f;
        return *this;
    }

    SamplerBuilder& SetMinFilter(Filter f)
    {
        m_info.MinFilter = f;
        return *this;
    }

    SamplerBuilder& SetFilter(Filter mag, Filter min)
    {
        m_info.MagFilter = mag;
        m_info.MinFilter = min;
        return *this;
    }

    SamplerBuilder& SetMipmapMode(SamplerMipmapMode mode)
    {
        m_info.MipmapMode = mode;
        return *this;
    }

    SamplerBuilder& SetAddressModeU(SamplerAddressMode mode)
    {
        m_info.AddressModeU = mode;
        return *this;
    }

    SamplerBuilder& SetAddressModeV(SamplerAddressMode mode)
    {
        m_info.AddressModeV = mode;
        return *this;
    }

    SamplerBuilder& SetAddressModeW(SamplerAddressMode mode)
    {
        m_info.AddressModeW = mode;
        return *this;
    }

    SamplerBuilder& SetAddressMode(SamplerAddressMode mode)
    {
        m_info.AddressModeU = m_info.AddressModeV = m_info.AddressModeW = mode;
        return *this;
    }

    SamplerBuilder& SetMipLodBias(float bias)
    {
        m_info.MipLodBias = bias;
        return *this;
    }

    SamplerBuilder& SetLodRange(float min, float max)
    {
        m_info.MinLod = min;
        m_info.MaxLod = max;
        return *this;
    }

    SamplerBuilder& SetAnisotropy(bool enable, float max = 16.0f)
    {
        m_info.AnisotropyEnable = enable;
        m_info.MaxAnisotropy = max;
        return *this;
    }

    SamplerBuilder& SetCompare(bool enable, CompareOp op = CompareOp::LessOrEqual)
    {
        m_info.CompareEnable = enable;
        m_info.CompareOp = op;
        return *this;
    }

    SamplerBuilder& SetBorderColor(BorderColor color)
    {
        m_info.BorderColor = color;
        return *this;
    }

    SamplerBuilder& SetUnnormalizedCoordinates(bool unnorm)
    {
        m_info.UnnormalizedCoordinates = unnorm;
        return *this;
    }

    SamplerBuilder& SetName(const std::string& name)
    {
        m_info.Name = name;
        return *this;
    }

    const SamplerCreateInfo& Build() const
    {
        return m_info;
    }

    operator const SamplerCreateInfo&() const
    {
        return m_info;
    }
};

class DescriptorSetLayoutBuilder {
    DescriptorSetLayoutCreateInfo m_info;

public:
    DescriptorSetLayoutBuilder& AddBinding(uint32_t binding,
                                           DescriptorType type,
                                           uint32_t count = 1,
                                           ShaderStage stages = ShaderStage::AllGraphics)
    {
        m_info.Bindings.push_back({binding, type, count, stages, {}});
        return *this;
    }

    DescriptorSetLayoutBuilder& AddBinding(const DescriptorSetLayoutBinding& binding)
    {
        m_info.Bindings.push_back(binding);
        return *this;
    }

    DescriptorSetLayoutBuilder& SetBindless(bool enable)
    {
        m_info.SupportBindless = enable;
        return *this;
    }

    const DescriptorSetLayoutCreateInfo& Build() const
    {
        return m_info;
    }

    operator const DescriptorSetLayoutCreateInfo&() const
    {
        return m_info;
    }
};

class DescriptorPoolBuilder {
    DescriptorPoolCreateInfo m_info;

public:
    DescriptorPoolBuilder& SetMaxSets(uint32_t max)
    {
        m_info.MaxSets = max;
        return *this;
    }

    DescriptorPoolBuilder& AddPoolSize(DescriptorType type, uint32_t count)
    {
        m_info.PoolSizes.push_back({type, count});
        return *this;
    }

    const DescriptorPoolCreateInfo& Build() const
    {
        return m_info;
    }

    operator const DescriptorPoolCreateInfo&() const
    {
        return m_info;
    }
};

class PipelineLayoutBuilder {
    PipelineLayoutCreateInfo m_info;

public:
    PipelineLayoutBuilder& AddSetLayout(const Ref<DescriptorSetLayout>& layout)
    {
        m_info.SetLayouts.push_back(layout);
        return *this;
    }

    PipelineLayoutBuilder& AddPushConstant(ShaderStage stages, uint32_t offset, uint32_t size)
    {
        m_info.PushConstantRanges.push_back({stages, offset, size});
        return *this;
    }

    const PipelineLayoutCreateInfo& Build() const
    {
        return m_info;
    }

    operator const PipelineLayoutCreateInfo&() const
    {
        return m_info;
    }
};

class GraphicsPipelineBuilder {
    GraphicsPipelineCreateInfo m_info;

public:
    GraphicsPipelineBuilder& AddShader(const Ref<ShaderModule>& shader)
    {
        m_info.Shaders.push_back(shader);
        return *this;
    }

    GraphicsPipelineBuilder& SetShaders(std::initializer_list<Ref<ShaderModule>> shaders)
    {
        m_info.Shaders = shaders;
        return *this;
    }

    GraphicsPipelineBuilder&
        AddVertexBinding(uint32_t binding, uint32_t stride, VertexInputRate rate = VertexInputRate::Vertex)
    {
        m_info.VertexBindings.push_back({binding, stride, rate});
        return *this;
    }

    GraphicsPipelineBuilder& AddVertexAttribute(uint32_t location,
                                                uint32_t binding,
                                                Format format,
                                                uint32_t offset,
                                                const std::string& semanticName = "TEXCOORD",
                                                uint32_t semanticIndex = UINT32_MAX)
    {
        m_info.VertexAttributes.push_back({location, binding, format, offset, semanticName, semanticIndex});
        return *this;
    }

    GraphicsPipelineBuilder& SetTopology(PrimitiveTopology topology)
    {
        m_info.InputAssembly.Topology = topology;
        return *this;
    }

    GraphicsPipelineBuilder& SetPrimitiveRestart(bool enable)
    {
        m_info.InputAssembly.PrimitiveRestartEnable = enable;
        return *this;
    }

    GraphicsPipelineBuilder& SetPolygonMode(PolygonMode mode)
    {
        m_info.Rasterizer.PolygonMode = mode;
        return *this;
    }

    GraphicsPipelineBuilder& SetCullMode(CullMode mode)
    {
        m_info.Rasterizer.CullMode = mode;
        return *this;
    }

    GraphicsPipelineBuilder& SetFrontFace(FrontFace face)
    {
        m_info.Rasterizer.FrontFace = face;
        return *this;
    }

    GraphicsPipelineBuilder& SetLineWidth(float width)
    {
        m_info.Rasterizer.LineWidth = width;
        return *this;
    }

    GraphicsPipelineBuilder& SetDepthClamp(bool enable)
    {
        m_info.Rasterizer.DepthClampEnable = enable;
        return *this;
    }

    GraphicsPipelineBuilder& SetRasterizerDiscard(bool enable)
    {
        m_info.Rasterizer.RasterizerDiscardEnable = enable;
        return *this;
    }

    GraphicsPipelineBuilder& SetDepthBias(bool enable, float constant = 0, float clamp = 0, float slope = 0)
    {
        m_info.Rasterizer.DepthBiasEnable = enable;
        m_info.Rasterizer.DepthBiasConstantFactor = constant;
        m_info.Rasterizer.DepthBiasClamp = clamp;
        m_info.Rasterizer.DepthBiasSlopeFactor = slope;
        return *this;
    }

    GraphicsPipelineBuilder& SetDepthTest(bool enable, bool write = true, CompareOp op = CompareOp::Less)
    {
        m_info.DepthStencil.DepthTestEnable = enable;
        m_info.DepthStencil.DepthWriteEnable = write;
        m_info.DepthStencil.DepthCompareOp = op;
        return *this;
    }

    GraphicsPipelineBuilder& SetStencilTest(bool enable)
    {
        m_info.DepthStencil.StencilTestEnable = enable;
        return *this;
    }

    GraphicsPipelineBuilder& SetSampleCount(uint32_t count)
    {
        m_info.Multisample.RasterizationSamples = count;
        return *this;
    }

    GraphicsPipelineBuilder& SetSampleShading(bool enable, float min = 0.2f)
    {
        m_info.Multisample.SampleShadingEnable = enable;
        m_info.Multisample.MinSampleShading = min;
        return *this;
    }

    GraphicsPipelineBuilder& SetAlphaToCoverage(bool enable)
    {
        m_info.Multisample.AlphaToCoverageEnable = enable;
        return *this;
    }

    GraphicsPipelineBuilder& AddColorAttachment(const ColorBlendAttachmentState& attachment)
    {
        m_info.ColorBlend.Attachments.push_back(attachment);
        return *this;
    }

    GraphicsPipelineBuilder& AddColorAttachmentDefault(bool blend = false)
    {
        ColorBlendAttachmentState att;
        att.BlendEnable = blend;
        if (blend) {
            att.SrcColorBlendFactor = BlendFactor::SrcAlpha;
            att.DstColorBlendFactor = BlendFactor::OneMinusSrcAlpha;
            att.ColorBlendOp = BlendOp::Add;
            att.SrcAlphaBlendFactor = BlendFactor::One;
            att.DstAlphaBlendFactor = BlendFactor::Zero;
            att.AlphaBlendOp = BlendOp::Add;
        }
        att.ColorWriteMask = ColorComponentFlags::All;
        m_info.ColorBlend.Attachments.push_back(att);
        return *this;
    }

    GraphicsPipelineBuilder& SetLogicOp(bool enable, LogicOp op = LogicOp::Copy)
    {
        m_info.ColorBlend.LogicOpEnable = enable;
        m_info.ColorBlend.LogicOp = op;
        return *this;
    }

    GraphicsPipelineBuilder& SetBlendConstants(float r, float g, float b, float a)
    {
        m_info.ColorBlend.BlendConstants[0] = r;
        m_info.ColorBlend.BlendConstants[1] = g;
        m_info.ColorBlend.BlendConstants[2] = b;
        m_info.ColorBlend.BlendConstants[3] = a;
        return *this;
    }

    GraphicsPipelineBuilder& AddColorFormat(Format format)
    {
        m_info.ColorAttachmentFormats.push_back(format);
        return *this;
    }

    GraphicsPipelineBuilder& SetDepthStencilFormat(Format format)
    {
        m_info.DepthStencilFormat = format;
        return *this;
    }

    GraphicsPipelineBuilder& SetLayout(const Ref<PipelineLayout>& layout)
    {
        m_info.Layout = layout;
        return *this;
    }

    GraphicsPipelineBuilder& SetCache(const Ref<PipelineCache>& cache)
    {
        m_info.Cache = cache;
        return *this;
    }

    const GraphicsPipelineCreateInfo& Build() const
    {
        return m_info;
    }

    operator const GraphicsPipelineCreateInfo&() const
    {
        return m_info;
    }
};

class ComputePipelineBuilder {
    ComputePipelineCreateInfo m_info;

public:
    ComputePipelineBuilder& SetShader(const Ref<ShaderModule>& shader)
    {
        m_info.ComputeShader = shader;
        return *this;
    }

    ComputePipelineBuilder& SetLayout(const Ref<PipelineLayout>& layout)
    {
        m_info.Layout = layout;
        return *this;
    }

    ComputePipelineBuilder& SetCache(const Ref<PipelineCache>& cache)
    {
        m_info.Cache = cache;
        return *this;
    }

    const ComputePipelineCreateInfo& Build() const
    {
        return m_info;
    }

    operator const ComputePipelineCreateInfo&() const
    {
        return m_info;
    }
};

class SwapchainBuilder {
    SwapchainCreateInfo m_info;

public:
    SwapchainBuilder& SetExtent(uint32_t w, uint32_t h)
    {
        m_info.Extent = {w, h};
        return *this;
    }

    SwapchainBuilder& SetExtent(Extent2D extent)
    {
        m_info.Extent = extent;
        return *this;
    }

    SwapchainBuilder& SetFormat(Format fmt)
    {
        m_info.Format = fmt;
        return *this;
    }

    SwapchainBuilder& SetColorSpace(ColorSpace cs)
    {
        m_info.ColorSpace = cs;
        return *this;
    }

    SwapchainBuilder& SetPresentMode(PresentMode mode)
    {
        m_info.PresentMode = mode;
        return *this;
    }

    SwapchainBuilder& SetMinImageCount(uint32_t count)
    {
        m_info.MinImageCount = count;
        return *this;
    }

    SwapchainBuilder& SetPreTransform(SurfaceTransform transform)
    {
        m_info.PreTransform = transform;
        return *this;
    }

    SwapchainBuilder& SetCompositeAlpha(CompositeAlpha alpha)
    {
        m_info.CompositeAlpha = alpha;
        return *this;
    }

    SwapchainBuilder& SetUsage(SwapchainUsageFlags usage)
    {
        m_info.Usage = usage;
        return *this;
    }

    SwapchainBuilder& SetClipped(bool clipped)
    {
        m_info.Clipped = clipped;
        return *this;
    }

    SwapchainBuilder& SetSurface(const Ref<Surface>& surface)
    {
        m_info.CompatibleSurface = surface;
        return *this;
    }

    SwapchainBuilder& SetImageArrayLayers(uint32_t layers)
    {
        m_info.ImageArrayLayers = layers;
        return *this;
    }

    const SwapchainCreateInfo& Build() const
    {
        return m_info;
    }

    operator const SwapchainCreateInfo&() const
    {
        return m_info;
    }
};
} // namespace Cacao
#endif
