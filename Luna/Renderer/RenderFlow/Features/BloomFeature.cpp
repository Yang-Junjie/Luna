#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Constants.h"
#include "Renderer/RenderFlow/Features/BloomFeature.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFeatureBindingContract.h"
#include "Renderer/RenderFlow/RenderFeatureRegistry.h"
#include "Renderer/RenderFlow/RenderFeatureResources.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/Resources/ShaderModuleLoader.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <Buffer.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <filesystem>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <string>
#include <string_view>
#include <Texture.h>
#include <utility>

namespace luna::render_flow {

LUNA_REGISTER_RENDER_FEATURE_EX(
    BloomFeature, "Bloom", "Bloom", "Post Processing", kDefaultRenderFlowName, true, true, 30)

void linkBloomFeature() {}

namespace {

inline constexpr std::string_view kFeatureName = "Bloom";
inline constexpr uint32_t kMaxBloomMipCount = 6;
inline constexpr uint32_t kBloomDescriptorSetCount = kMaxBloomMipCount * 2 + 2;
inline constexpr uint32_t kBloomDebugDescriptorSetIndex = kBloomDescriptorSetCount - 2;
inline constexpr uint32_t kBloomCompositeDescriptorSetIndex = kBloomDescriptorSetCount - 1;

constexpr std::array<RenderFeatureGraphResource, 1> kGraphInputs{{
    {.name = blackboard::SceneTransparentCompositedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
}};

constexpr std::array<RenderFeatureGraphResource, 1> kGraphOutputs{{
    {.name = blackboard::SceneBloomCompositedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
}};

constexpr std::array<RenderPassResourceUsage, 2> kBloomPassResources{{
    {.name = blackboard::SceneTransparentCompositedColor.value(),
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneBloomCompositedColor.value(),
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

struct BloomGpuParams {
    glm::vec4 target_size;
    glm::vec4 filter_params;
    glm::vec4 debug_params;
};

namespace bloom_binding {
constexpr uint32_t SourceTexture = 0;
constexpr uint32_t BloomTexture = 1;
constexpr uint32_t Sampler = 2;
constexpr uint32_t Params = 3;
} // namespace bloom_binding

constexpr std::array<RenderFeatureDescriptorBinding, 4> kBloomBindings{{
    {"SourceTexture",
     "gBloomSourceTexture",
     bloom_binding::SourceTexture,
     RHI::DescriptorType::SampledImage,
     1,
     RHI::ShaderStage::Fragment},
    {"BloomTexture",
     "gBloomTexture",
     bloom_binding::BloomTexture,
     RHI::DescriptorType::SampledImage,
     1,
     RHI::ShaderStage::Fragment},
    {"Sampler",
     "gBloomSampler",
     bloom_binding::Sampler,
     RHI::DescriptorType::Sampler,
     1,
     RHI::ShaderStage::Fragment},
    {"Params",
     "gBloomParams",
     bloom_binding::Params,
     RHI::DescriptorType::UniformBuffer,
     1,
     RHI::ShaderStage::Fragment},
}};

bool isValidTextureHandle(const std::optional<RenderGraphTextureHandle>& handle)
{
    return handle.has_value() && handle->isValid();
}

bool isBloomDebugView(RenderDebugViewMode mode) noexcept
{
    switch (mode) {
        case RenderDebugViewMode::BloomInput:
        case RenderDebugViewMode::BloomPrefilter:
        case RenderDebugViewMode::BloomMip0:
        case RenderDebugViewMode::BloomMip1:
        case RenderDebugViewMode::BloomMip2:
        case RenderDebugViewMode::BloomMip3:
        case RenderDebugViewMode::BloomMip4:
        case RenderDebugViewMode::BloomMip5:
        case RenderDebugViewMode::BloomComposite:
            return true;
        default:
            return false;
    }
}

bool requiresBloomDebugPipeline(const SceneRenderContext& context) noexcept
{
    return isBloomDebugView(context.debug_view_mode) && context.debug_target.isValid() &&
           context.debug_format != RHI::Format::UNDEFINED;
}

std::optional<uint32_t> bloomDebugMipIndex(RenderDebugViewMode mode) noexcept
{
    switch (mode) {
        case RenderDebugViewMode::BloomMip0:
            return 0u;
        case RenderDebugViewMode::BloomMip1:
            return 1u;
        case RenderDebugViewMode::BloomMip2:
            return 2u;
        case RenderDebugViewMode::BloomMip3:
            return 3u;
        case RenderDebugViewMode::BloomMip4:
            return 4u;
        case RenderDebugViewMode::BloomMip5:
            return 5u;
        default:
            return std::nullopt;
    }
}

float bloomDebugPreviewExposure(RenderDebugViewMode mode) noexcept
{
    return (mode == RenderDebugViewMode::BloomInput || mode == RenderDebugViewMode::BloomComposite) ? 1.0f : 4.0f;
}

std::filesystem::path shaderPath()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders" / "Bloom.slang";
}

RHI::DescriptorSetLayoutCreateInfo makeBloomDescriptorSetLayoutCreateInfo()
{
    return makeRenderFeatureDescriptorSetLayoutCreateInfo(kBloomBindings);
}

ShaderBindingContract makeBloomShaderBindingContract()
{
    return makeRenderFeatureShaderBindingContract(RenderFeatureDescriptorSetContract{
        .contract_name = kFeatureName,
        .set_name = kFeatureName,
        .logical_set = 0,
        .set = 0,
        .bindings = kBloomBindings,
    });
}

RHI::ColorBlendAttachmentState makeAdditiveBlendAttachment()
{
    RHI::ColorBlendAttachmentState blend_attachment{};
    blend_attachment.BlendEnable = true;
    blend_attachment.SrcColorBlendFactor = RHI::BlendFactor::One;
    blend_attachment.DstColorBlendFactor = RHI::BlendFactor::One;
    blend_attachment.ColorBlendOp = RHI::BlendOp::Add;
    blend_attachment.SrcAlphaBlendFactor = RHI::BlendFactor::One;
    blend_attachment.DstAlphaBlendFactor = RHI::BlendFactor::One;
    blend_attachment.AlphaBlendOp = RHI::BlendOp::Add;
    blend_attachment.ColorWriteMask = RHI::ColorComponentFlags::All;
    return blend_attachment;
}

RHI::Ref<RHI::DescriptorSetLayout>
    createDescriptorSetLayout(const RHI::Ref<RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(makeBloomDescriptorSetLayoutCreateInfo());
}

RHI::Ref<RHI::DescriptorPool> createDescriptorPool(const RHI::Ref<RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorPool(RHI::DescriptorPoolBuilder()
                                            .SetMaxSets(32)
                                            .AddPoolSize(RHI::DescriptorType::SampledImage, 96)
                                            .AddPoolSize(RHI::DescriptorType::Sampler, 32)
                                            .AddPoolSize(RHI::DescriptorType::UniformBuffer, 32)
                                            .Build());
}

RHI::Ref<RHI::Sampler> createSampler(const RHI::Ref<RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateSampler(RHI::SamplerBuilder()
                                     .SetFilter(RHI::Filter::Linear, RHI::Filter::Linear)
                                     .SetMipmapMode(RHI::SamplerMipmapMode::Nearest)
                                     .SetAddressMode(RHI::SamplerAddressMode::ClampToEdge)
                                     .SetAnisotropy(false)
                                     .SetName("BloomSampler")
                                     .Build());
}

RHI::Ref<RHI::PipelineLayout>
    createPipelineLayout(const RHI::Ref<RHI::Device>& device,
                         const RHI::Ref<RHI::DescriptorSetLayout>& layout)
{
    if (!device || !layout) {
        return {};
    }

    return device->CreatePipelineLayout(RHI::PipelineLayoutBuilder().AddSetLayout(layout).Build());
}

RHI::Ref<RHI::GraphicsPipeline>
    createPipeline(const RHI::Ref<RHI::Device>& device,
                   const RHI::Ref<RHI::PipelineLayout>& layout,
                   const RHI::Ref<RHI::ShaderModule>& vertex_shader,
                   const RHI::Ref<RHI::ShaderModule>& fragment_shader,
                   RHI::Format color_format,
                   bool additive_blend)
{
    if (!device || !layout || !vertex_shader || !fragment_shader || color_format == RHI::Format::UNDEFINED) {
        return {};
    }

    RHI::GraphicsPipelineBuilder builder;
    builder.SetShaders({vertex_shader, fragment_shader})
        .SetTopology(RHI::PrimitiveTopology::TriangleList)
        .SetCullMode(RHI::CullMode::None)
        .SetFrontFace(RHI::FrontFace::CounterClockwise)
        .SetDepthTest(false, false, RHI::CompareOp::Always)
        .SetLayout(layout);
    if (additive_blend) {
        builder.AddColorAttachment(makeAdditiveBlendAttachment());
    } else {
        builder.AddColorAttachmentDefault(false);
    }
    builder.AddColorFormat(color_format);
    return device->CreateGraphicsPipeline(builder.Build());
}

uint32_t halfExtent(uint32_t value)
{
    return std::max(1u, (value + 1u) / 2u);
}

uint32_t bloomMipCount(const SceneRenderContext& scene_context, const BloomOptions& options)
{
    uint32_t width = halfExtent(scene_context.framebuffer_width);
    uint32_t height = halfExtent(scene_context.framebuffer_height);
    uint32_t max_mips = 1;
    while (max_mips < kMaxBloomMipCount && (width > 1 || height > 1)) {
        width = halfExtent(width);
        height = halfExtent(height);
        ++max_mips;
    }
    return std::clamp(static_cast<uint32_t>(std::max(options.mip_count, 1)), 1u, max_mips);
}

RenderGraphTextureDesc makeBloomTextureDesc(std::string name, uint32_t width, uint32_t height)
{
    return RenderGraphTextureDesc{
        .Name = std::move(name),
        .Type = RHI::TextureType::Texture2D,
        .Width = std::max(width, 1u),
        .Height = std::max(height, 1u),
        .Depth = 1,
        .ArrayLayers = 1,
        .MipLevels = 1,
        .Format = default_scene_detail::kSceneHdrColorFormat,
        .Usage = RHI::TextureUsageFlags::ColorAttachment | RHI::TextureUsageFlags::Sampled,
        .InitialState = RHI::ResourceState::Undefined,
        .SampleCount = RHI::SampleCount::Count1,
    };
}

RenderFeatureParameterInfo makeFloatParameter(
    std::string_view name, std::string_view display_name, float value, float min_value, float max_value, float step)
{
    RenderFeatureParameterInfo parameter{};
    parameter.name = name;
    parameter.display_name = display_name;
    parameter.type = RenderFeatureParameterType::Float;
    parameter.value.type = RenderFeatureParameterType::Float;
    parameter.value.float_value = value;
    parameter.min.type = RenderFeatureParameterType::Float;
    parameter.min.float_value = min_value;
    parameter.max.type = RenderFeatureParameterType::Float;
    parameter.max.float_value = max_value;
    parameter.step = step;
    return parameter;
}

RenderFeatureParameterInfo makeIntParameter(
    std::string_view name, std::string_view display_name, int32_t value, int32_t min_value, int32_t max_value)
{
    RenderFeatureParameterInfo parameter{};
    parameter.name = name;
    parameter.display_name = display_name;
    parameter.type = RenderFeatureParameterType::Int;
    parameter.value.type = RenderFeatureParameterType::Int;
    parameter.value.int_value = value;
    parameter.min.type = RenderFeatureParameterType::Int;
    parameter.min.int_value = min_value;
    parameter.max.type = RenderFeatureParameterType::Int;
    parameter.max.int_value = max_value;
    parameter.step = 1.0f;
    return parameter;
}

void clearBloomPass(RenderGraphRasterPassContext& pass_context)
{
    pass_context.beginRendering();
    pass_context.endRendering();
}

} // namespace

class BloomFeature::Resources final {
public:
    void shutdown()
    {
        releasePipelineResources();
        m_resource_set.resetGpuContext();
        m_resource_set.resetBindingContractDiagnostics();
    }

    void releasePipelineResources() noexcept
    {
        m_state = {};
    }

    [[nodiscard]] bool ensure(const SceneRenderContext& context)
    {
        const RenderFeatureGpuResourceDecision decision =
            m_resource_set.prepareGpuResourceBuild(context, isComplete(context));
        if (decision.action == RenderFeatureGpuResourceAction::InvalidContext) {
            return false;
        }
        if (decision.action == RenderFeatureGpuResourceAction::Reuse) {
            return true;
        }

        releasePipelineResources();
        const RHI::Ref<RHI::Device>& device = m_resource_set.device();

        const std::filesystem::path path = shaderPath();
        const bool debug_required = requiresBloomDebugPipeline(context);
        m_state.vertex_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "bloomVertexMain", RHI::ShaderStage::Vertex);
        m_state.prefilter_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "bloomPrefilterFragmentMain", RHI::ShaderStage::Fragment);
        m_state.downsample_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "bloomDownsampleFragmentMain", RHI::ShaderStage::Fragment);
        m_state.upsample_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "bloomUpsampleFragmentMain", RHI::ShaderStage::Fragment);
        m_state.composite_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "bloomCompositeFragmentMain", RHI::ShaderStage::Fragment);
        if (debug_required) {
            m_state.debug_shader = renderer_detail::loadShaderModule(
                device, context.compiler, path, "bloomDebugFragmentMain", RHI::ShaderStage::Fragment);
        }

        const ShaderBindingContract contract = makeBloomShaderBindingContract();
        if (debug_required) {
            const std::array<RenderFeatureShaderBindingCheck, 6> binding_checks{{
                {.shader = m_state.vertex_shader, .entry_point = "bloomVertexMain"},
                {.shader = m_state.prefilter_shader, .entry_point = "bloomPrefilterFragmentMain"},
                {.shader = m_state.downsample_shader, .entry_point = "bloomDownsampleFragmentMain"},
                {.shader = m_state.upsample_shader, .entry_point = "bloomUpsampleFragmentMain"},
                {.shader = m_state.composite_shader, .entry_point = "bloomCompositeFragmentMain"},
                {.shader = m_state.debug_shader, .entry_point = "bloomDebugFragmentMain"},
            }};
            m_resource_set.validateShaderBindingContract(binding_checks, contract, path);
        } else {
            const std::array<RenderFeatureShaderBindingCheck, 5> binding_checks{{
                {.shader = m_state.vertex_shader, .entry_point = "bloomVertexMain"},
                {.shader = m_state.prefilter_shader, .entry_point = "bloomPrefilterFragmentMain"},
                {.shader = m_state.downsample_shader, .entry_point = "bloomDownsampleFragmentMain"},
                {.shader = m_state.upsample_shader, .entry_point = "bloomUpsampleFragmentMain"},
                {.shader = m_state.composite_shader, .entry_point = "bloomCompositeFragmentMain"},
            }};
            m_resource_set.validateShaderBindingContract(binding_checks, contract, path);
        }

        m_state.layout = createDescriptorSetLayout(device);
        m_state.descriptor_pool = createDescriptorPool(device);
        m_state.pipeline_layout = createPipelineLayout(device, m_state.layout);
        m_state.prefilter_pipeline = createPipeline(device,
                                                    m_state.pipeline_layout,
                                                    m_state.vertex_shader,
                                                    m_state.prefilter_shader,
                                                    default_scene_detail::kSceneHdrColorFormat,
                                                    false);
        m_state.downsample_pipeline = createPipeline(device,
                                                     m_state.pipeline_layout,
                                                     m_state.vertex_shader,
                                                     m_state.downsample_shader,
                                                     default_scene_detail::kSceneHdrColorFormat,
                                                     false);
        m_state.upsample_pipeline = createPipeline(device,
                                                   m_state.pipeline_layout,
                                                   m_state.vertex_shader,
                                                   m_state.upsample_shader,
                                                   default_scene_detail::kSceneHdrColorFormat,
                                                   true);
        m_state.composite_pipeline = createPipeline(device,
                                                    m_state.pipeline_layout,
                                                    m_state.vertex_shader,
                                                    m_state.composite_shader,
                                                    default_scene_detail::kSceneHdrColorFormat,
                                                    false);
        if (debug_required) {
            m_state.debug_pipeline = createPipeline(device,
                                                    m_state.pipeline_layout,
                                                    m_state.vertex_shader,
                                                    m_state.debug_shader,
                                                    context.debug_format,
                                                    false);
            m_state.debug_format = context.debug_format;
        }
        m_state.sampler = createSampler(device);
        for (uint32_t index = 0; index < m_state.params_buffers.size(); ++index) {
            m_state.params_buffers[index] =
                device->CreateBuffer(RHI::BufferBuilder()
                                         .SetSize(sizeof(BloomGpuParams))
                                         .SetUsage(RHI::BufferUsageFlags::UniformBuffer)
                                         .SetMemoryUsage(RHI::BufferMemoryUsage::CpuToGpu)
                                         .SetName("BloomParams" + std::to_string(index))
                                         .Build());
        }

        if (m_state.descriptor_pool && m_state.layout) {
            for (auto& descriptor_set : m_state.descriptor_sets) {
                descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.layout);
            }
        }

        return m_resource_set.logGpuResourceBuildResult(resourceStatus(context));
    }

    [[nodiscard]] bool isComplete(const SceneRenderContext& context) const noexcept
    {
        return hasCoreResources() && hasDebugResources(context);
    }

    [[nodiscard]] bool hasCoreResources() const noexcept
    {
        return m_resource_set.hasGpuContext() && m_state.vertex_shader && m_state.prefilter_shader &&
               m_state.downsample_shader && m_state.upsample_shader && m_state.composite_shader && m_state.layout &&
               m_state.descriptor_pool && m_state.pipeline_layout && m_state.prefilter_pipeline &&
               m_state.downsample_pipeline && m_state.upsample_pipeline && m_state.composite_pipeline &&
               m_state.sampler && allParamsBuffersReady() && allDescriptorSetsReady();
    }

    [[nodiscard]] bool hasDebugResources(const SceneRenderContext& context) const noexcept
    {
        return !requiresBloomDebugPipeline(context) ||
               (m_state.debug_shader && m_state.debug_pipeline && m_state.debug_format == context.debug_format);
    }

    void updateBindings(uint32_t descriptor_set_index,
                        const RHI::Ref<RHI::Texture>& source,
                        const RHI::Ref<RHI::Texture>& bloom,
                        uint32_t width,
                        uint32_t height,
                        const Options& options,
                        RenderDebugViewMode debug_view_mode = RenderDebugViewMode::None)
    {
        if (!hasCoreResources() || descriptor_set_index >= m_state.descriptor_sets.size() ||
            descriptor_set_index >= m_state.params_buffers.size() || !source || width == 0 || height == 0) {
            return;
        }

        const RHI::Ref<RHI::Texture>& bloom_texture = bloom ? bloom : source;
        updateParams(descriptor_set_index, width, height, options, debug_view_mode);
        auto& descriptor_set = m_state.descriptor_sets[descriptor_set_index];
        descriptor_set->WriteTexture(RHI::TextureWriteInfo{
            .Binding = bloom_binding::SourceTexture,
            .TextureView = source->GetDefaultView(),
            .Layout = RHI::ResourceState::ShaderRead,
            .Type = RHI::DescriptorType::SampledImage,
        });
        descriptor_set->WriteTexture(RHI::TextureWriteInfo{
            .Binding = bloom_binding::BloomTexture,
            .TextureView = bloom_texture->GetDefaultView(),
            .Layout = RHI::ResourceState::ShaderRead,
            .Type = RHI::DescriptorType::SampledImage,
        });
        descriptor_set->WriteSampler(RHI::SamplerWriteInfo{
            .Binding = bloom_binding::Sampler,
            .Sampler = m_state.sampler,
        });
        descriptor_set->WriteBuffer(RHI::BufferWriteInfo{
            .Binding = bloom_binding::Params,
            .Buffer = m_state.params_buffers[descriptor_set_index],
            .Offset = 0,
            .Stride = sizeof(BloomGpuParams),
            .Size = sizeof(BloomGpuParams),
            .Type = RHI::DescriptorType::UniformBuffer,
        });
        descriptor_set->Update();
    }

    void draw(RenderGraphRasterPassContext& pass_context,
              const RHI::Ref<RHI::GraphicsPipeline>& pipeline,
              uint32_t descriptor_set_index) const
    {
        if (!hasCoreResources() || !pipeline || descriptor_set_index >= m_state.descriptor_sets.size()) {
            clearBloomPass(pass_context);
            return;
        }

        pass_context.beginRendering();
        auto& commands = pass_context.commandBuffer();
        commands.BindGraphicsPipeline(pipeline);
        commands.SetViewport({0.0f,
                              0.0f,
                              static_cast<float>(pass_context.framebufferWidth()),
                              static_cast<float>(pass_context.framebufferHeight()),
                              0.0f,
                              1.0f});
        commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});
        const std::array<RHI::Ref<RHI::DescriptorSet>, 1> descriptor_sets{
            m_state.descriptor_sets[descriptor_set_index]};
        commands.BindDescriptorSets(pipeline, 0, descriptor_sets);
        commands.Draw(3, 1, 0, 0);
        pass_context.endRendering();
    }

    [[nodiscard]] const RHI::Ref<RHI::GraphicsPipeline>& prefilterPipeline() const noexcept
    {
        return m_state.prefilter_pipeline;
    }

    [[nodiscard]] const RHI::Ref<RHI::GraphicsPipeline>& downsamplePipeline() const noexcept
    {
        return m_state.downsample_pipeline;
    }

    [[nodiscard]] const RHI::Ref<RHI::GraphicsPipeline>& upsamplePipeline() const noexcept
    {
        return m_state.upsample_pipeline;
    }

    [[nodiscard]] const RHI::Ref<RHI::GraphicsPipeline>& compositePipeline() const noexcept
    {
        return m_state.composite_pipeline;
    }

    [[nodiscard]] const RHI::Ref<RHI::GraphicsPipeline>& debugPipeline() const noexcept
    {
        return m_state.debug_pipeline;
    }

    [[nodiscard]] RenderFeatureDiagnostics diagnostics() const
    {
        RenderFeatureDiagnostics result;
        m_resource_set.writeBindingContractDiagnostics(result);
        m_resource_set.writePipelineResourceDiagnostics(result, hasCoreResources(), resourceStatus());
        return result;
    }

private:
    [[nodiscard]] bool allDescriptorSetsReady() const noexcept
    {
        return std::all_of(
            m_state.descriptor_sets.begin(), m_state.descriptor_sets.end(), [](const auto& descriptor_set) {
                return static_cast<bool>(descriptor_set);
            });
    }

    [[nodiscard]] bool allParamsBuffersReady() const noexcept
    {
        return std::all_of(m_state.params_buffers.begin(), m_state.params_buffers.end(), [](const auto& params_buffer) {
            return static_cast<bool>(params_buffer);
        });
    }

    void updateParams(uint32_t descriptor_set_index,
                      uint32_t width,
                      uint32_t height,
                      const Options& options,
                      RenderDebugViewMode debug_view_mode)
    {
        if (descriptor_set_index >= m_state.params_buffers.size() || !m_state.params_buffers[descriptor_set_index]) {
            return;
        }

        const float target_width = static_cast<float>(std::max(width, 1u));
        const float target_height = static_cast<float>(std::max(height, 1u));
        const BloomGpuParams params{
            .target_size = glm::vec4(target_width, target_height, 1.0f / target_width, 1.0f / target_height),
            .filter_params = glm::vec4(std::max(options.threshold, 0.0f),
                                       std::clamp(options.soft_knee, 0.0f, 1.0f),
                                       std::max(options.intensity, 0.0f),
                                       std::clamp(options.radius, 0.01f, 4.0f)),
            .debug_params = glm::vec4(static_cast<float>(debug_view_mode),
                                      bloomDebugPreviewExposure(debug_view_mode),
                                      (debug_view_mode == RenderDebugViewMode::BloomInput ||
                                       debug_view_mode == RenderDebugViewMode::BloomComposite)
                                          ? 1.0f
                                          : 0.0f,
                                      0.0f),
        };
        if (void* mapped = m_state.params_buffers[descriptor_set_index]->Map()) {
            std::memcpy(mapped, &params, sizeof(params));
            m_state.params_buffers[descriptor_set_index]->Flush();
            m_state.params_buffers[descriptor_set_index]->Unmap();
        }
    }

    struct State {
        RHI::Ref<RHI::DescriptorSetLayout> layout;
        RHI::Ref<RHI::DescriptorPool> descriptor_pool;
        RHI::Ref<RHI::PipelineLayout> pipeline_layout;
        RHI::Ref<RHI::GraphicsPipeline> prefilter_pipeline;
        RHI::Ref<RHI::GraphicsPipeline> downsample_pipeline;
        RHI::Ref<RHI::GraphicsPipeline> upsample_pipeline;
        RHI::Ref<RHI::GraphicsPipeline> composite_pipeline;
        RHI::Ref<RHI::GraphicsPipeline> debug_pipeline;
        RHI::Ref<RHI::Sampler> sampler;
        RHI::Format debug_format{RHI::Format::UNDEFINED};
        std::array<RHI::Ref<RHI::Buffer>, kBloomDescriptorSetCount> params_buffers;
        std::array<RHI::Ref<RHI::DescriptorSet>, kBloomDescriptorSetCount> descriptor_sets;
        RHI::Ref<RHI::ShaderModule> vertex_shader;
        RHI::Ref<RHI::ShaderModule> prefilter_shader;
        RHI::Ref<RHI::ShaderModule> downsample_shader;
        RHI::Ref<RHI::ShaderModule> upsample_shader;
        RHI::Ref<RHI::ShaderModule> composite_shader;
        RHI::Ref<RHI::ShaderModule> debug_shader;
    };

    [[nodiscard]] std::array<RenderFeatureResourceStatus, 17> resourceStatus() const noexcept
    {
        const bool debug_requested = m_state.debug_format != RHI::Format::UNDEFINED;
        return {{
            {"vertex_shader", static_cast<bool>(m_state.vertex_shader)},
            {"prefilter_shader", static_cast<bool>(m_state.prefilter_shader)},
            {"downsample_shader", static_cast<bool>(m_state.downsample_shader)},
            {"upsample_shader", static_cast<bool>(m_state.upsample_shader)},
            {"composite_shader", static_cast<bool>(m_state.composite_shader)},
            {"debug_shader", !debug_requested || static_cast<bool>(m_state.debug_shader)},
            {"layout", static_cast<bool>(m_state.layout)},
            {"descriptor_pool", static_cast<bool>(m_state.descriptor_pool)},
            {"pipeline_layout", static_cast<bool>(m_state.pipeline_layout)},
            {"prefilter_pipeline", static_cast<bool>(m_state.prefilter_pipeline)},
            {"downsample_pipeline", static_cast<bool>(m_state.downsample_pipeline)},
            {"upsample_pipeline", static_cast<bool>(m_state.upsample_pipeline)},
            {"composite_pipeline", static_cast<bool>(m_state.composite_pipeline)},
            {"debug_pipeline", !debug_requested || static_cast<bool>(m_state.debug_pipeline)},
            {"sampler", static_cast<bool>(m_state.sampler)},
            {"params_buffers", allParamsBuffersReady()},
            {"descriptor_sets", allDescriptorSetsReady()},
        }};
    }

    [[nodiscard]] std::array<RenderFeatureResourceStatus, 17>
        resourceStatus(const SceneRenderContext& context) const noexcept
    {
        auto status = resourceStatus();
        if (!requiresBloomDebugPipeline(context)) {
            status[5].ready = true;
            status[13].ready = true;
        }
        return status;
    }

    State m_state{};
    RenderFeatureResourceSet m_resource_set{std::string(kFeatureName)};
};

namespace {

class BloomPass final : public IRenderPass {
public:
    BloomPass(BloomFeature::Resources& resources, BloomFeature::OptionsHandle options)
        : m_resources(&resources),
          m_options(std::move(options))
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "Bloom";
    }

    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override
    {
        return kBloomPassResources;
    }

    void setup(RenderPassContext& context) override
    {
        const SceneRenderContext& scene_context = context.sceneContext();
        const BloomFeature::Options options = currentOptions();
        const auto source = context.blackboard().get(blackboard::SceneTransparentCompositedColor);
        if (!isValidTextureHandle(source)) {
            LUNA_RENDERER_WARN("Bloom missing input texture '{}'", blackboard::SceneTransparentCompositedColor.value());
            return;
        }

        blackboard::publishSceneColorStage(context.blackboard(), blackboard::SceneColorStage::BloomComposited, *source);

        const bool resources_ready = m_resources != nullptr && m_resources->ensure(scene_context);
        if (!resources_ready) {
            return;
        }

        const uint32_t mip_count = bloomMipCount(scene_context, options);
        std::array<RenderGraphTextureHandle, kMaxBloomMipCount> bloom_levels{};
        uint32_t width = halfExtent(scene_context.framebuffer_width);
        uint32_t height = halfExtent(scene_context.framebuffer_height);
        for (uint32_t mip_index = 0; mip_index < mip_count; ++mip_index) {
            bloom_levels[mip_index] = context.graph().CreateTexture(
                makeBloomTextureDesc("BloomMip" + std::to_string(mip_index), width, height));
            if (!bloom_levels[mip_index].isValid()) {
                return;
            }
            width = halfExtent(width);
            height = halfExtent(height);
        }

        const RenderGraphTextureHandle output = context.graph().CreateTexture(makeBloomTextureDesc(
            "SceneBloomCompositedColor", scene_context.framebuffer_width, scene_context.framebuffer_height));
        if (!output.isValid()) {
            return;
        }
        blackboard::publishSceneColorStage(context.blackboard(), blackboard::SceneColorStage::BloomComposited, output);

        if (scene_context.debug_view_mode == RenderDebugViewMode::BloomInput) {
            addDebugPass(context, "BloomDebugInput", *source, options);
        }

        addPrefilterPass(context, *source, bloom_levels[0], options);
        if (scene_context.debug_view_mode == RenderDebugViewMode::BloomPrefilter) {
            addDebugPass(context, "BloomDebugPrefilter", bloom_levels[0], options);
        }
        for (uint32_t mip_index = 1; mip_index < mip_count; ++mip_index) {
            addDownsamplePass(context, bloom_levels[mip_index - 1], bloom_levels[mip_index], options, mip_index);
        }
        for (uint32_t mip_index = mip_count - 1; mip_index > 0; --mip_index) {
            addUpsamplePass(context, bloom_levels[mip_index], bloom_levels[mip_index - 1], options, mip_index - 1);
        }
        if (const std::optional<uint32_t> debug_mip_index = bloomDebugMipIndex(scene_context.debug_view_mode);
            debug_mip_index.has_value() && *debug_mip_index < mip_count) {
            addDebugPass(
                context, "BloomDebugMip" + std::to_string(*debug_mip_index), bloom_levels[*debug_mip_index], options);
        }
        addCompositePass(context, *source, bloom_levels[0], output, options);
        if (scene_context.debug_view_mode == RenderDebugViewMode::BloomComposite) {
            addDebugPass(context, "BloomDebugComposite", output, options);
        }
    }

private:
    void addPrefilterPass(RenderPassContext& context,
                          RenderGraphTextureHandle source,
                          RenderGraphTextureHandle destination,
                          BloomFeature::Options options)
    {
        context.graph().AddRasterPass(
            "BloomPrefilter",
            [source, destination](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(source);
                pass_builder.WriteColor(destination,
                                        RHI::AttachmentLoadOp::Clear,
                                        RHI::AttachmentStoreOp::Store,
                                        RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, source, options](RenderGraphRasterPassContext& pass_context) {
                const auto& source_texture = pass_context.getTexture(source);
                if (!source_texture || m_resources == nullptr) {
                    clearBloomPass(pass_context);
                    return;
                }
                m_resources->updateBindings(0,
                                            source_texture,
                                            source_texture,
                                            pass_context.framebufferWidth(),
                                            pass_context.framebufferHeight(),
                                            options);
                m_resources->draw(pass_context, m_resources->prefilterPipeline(), 0);
            });
    }

    void addDownsamplePass(RenderPassContext& context,
                           RenderGraphTextureHandle source,
                           RenderGraphTextureHandle destination,
                           BloomFeature::Options options,
                           uint32_t mip_index)
    {
        const uint32_t descriptor_set_index = 1 + mip_index;
        context.graph().AddRasterPass(
            "BloomDownsample" + std::to_string(mip_index),
            [source, destination](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(source);
                pass_builder.WriteColor(destination,
                                        RHI::AttachmentLoadOp::Clear,
                                        RHI::AttachmentStoreOp::Store,
                                        RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, source, options, descriptor_set_index](RenderGraphRasterPassContext& pass_context) {
                const auto& source_texture = pass_context.getTexture(source);
                if (!source_texture || m_resources == nullptr) {
                    clearBloomPass(pass_context);
                    return;
                }
                m_resources->updateBindings(descriptor_set_index,
                                            source_texture,
                                            source_texture,
                                            pass_context.framebufferWidth(),
                                            pass_context.framebufferHeight(),
                                            options);
                m_resources->draw(pass_context, m_resources->downsamplePipeline(), descriptor_set_index);
            });
    }

    void addUpsamplePass(RenderPassContext& context,
                         RenderGraphTextureHandle source,
                         RenderGraphTextureHandle destination,
                         BloomFeature::Options options,
                         uint32_t mip_index)
    {
        const uint32_t descriptor_set_index = 1 + kMaxBloomMipCount + mip_index;
        context.graph().AddRasterPass(
            "BloomUpsample" + std::to_string(mip_index),
            [source, destination](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(source);
                pass_builder.WriteColor(
                    destination, RHI::AttachmentLoadOp::Load, RHI::AttachmentStoreOp::Store);
            },
            [this, source, options, descriptor_set_index](RenderGraphRasterPassContext& pass_context) {
                const auto& source_texture = pass_context.getTexture(source);
                if (!source_texture || m_resources == nullptr) {
                    clearBloomPass(pass_context);
                    return;
                }
                m_resources->updateBindings(descriptor_set_index,
                                            source_texture,
                                            source_texture,
                                            pass_context.framebufferWidth(),
                                            pass_context.framebufferHeight(),
                                            options);
                m_resources->draw(pass_context, m_resources->upsamplePipeline(), descriptor_set_index);
            });
    }

    void addCompositePass(RenderPassContext& context,
                          RenderGraphTextureHandle source,
                          RenderGraphTextureHandle bloom,
                          RenderGraphTextureHandle destination,
                          BloomFeature::Options options)
    {
        context.graph().AddRasterPass(
            "BloomComposite",
            [source, bloom, destination](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(source);
                pass_builder.ReadTexture(bloom);
                pass_builder.WriteColor(destination,
                                        RHI::AttachmentLoadOp::Clear,
                                        RHI::AttachmentStoreOp::Store,
                                        RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, source, bloom, options](RenderGraphRasterPassContext& pass_context) {
                const auto& source_texture = pass_context.getTexture(source);
                const auto& bloom_texture = pass_context.getTexture(bloom);
                if (!source_texture || !bloom_texture || m_resources == nullptr) {
                    clearBloomPass(pass_context);
                    return;
                }
                m_resources->updateBindings(kBloomCompositeDescriptorSetIndex,
                                            source_texture,
                                            bloom_texture,
                                            pass_context.framebufferWidth(),
                                            pass_context.framebufferHeight(),
                                            options);
                m_resources->draw(pass_context, m_resources->compositePipeline(), kBloomCompositeDescriptorSetIndex);
            });
    }

    void addDebugPass(RenderPassContext& context,
                      std::string pass_name,
                      RenderGraphTextureHandle source,
                      BloomFeature::Options options)
    {
        const SceneRenderContext scene_context = context.sceneContext();
        if (!requiresBloomDebugPipeline(scene_context)) {
            return;
        }

        context.graph().AddRasterPass(
            pass_name,
            [source, scene_context](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(source);
                pass_builder.WriteColor(scene_context.debug_target,
                                        RHI::AttachmentLoadOp::Clear,
                                        RHI::AttachmentStoreOp::Store,
                                        RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, source, options, debug_view_mode = scene_context.debug_view_mode](
                RenderGraphRasterPassContext& pass_context) {
                const auto& source_texture = pass_context.getTexture(source);
                if (!source_texture || m_resources == nullptr) {
                    clearBloomPass(pass_context);
                    return;
                }
                m_resources->updateBindings(kBloomDebugDescriptorSetIndex,
                                            source_texture,
                                            source_texture,
                                            pass_context.framebufferWidth(),
                                            pass_context.framebufferHeight(),
                                            options,
                                            debug_view_mode);
                m_resources->draw(pass_context, m_resources->debugPipeline(), kBloomDebugDescriptorSetIndex);
            });
    }

    [[nodiscard]] BloomFeature::Options currentOptions() const
    {
        return m_options ? *m_options : BloomFeature::Options{};
    }

    BloomFeature::Resources* m_resources{nullptr};
    BloomFeature::OptionsHandle m_options;
};

} // namespace

BloomFeature::BloomFeature()
    : BloomFeature(std::make_shared<Options>())
{}

BloomFeature::BloomFeature(Options options)
    : BloomFeature(std::make_shared<Options>(options))
{}

BloomFeature::BloomFeature(OptionsHandle options)
    : m_options(std::move(options)),
      m_resources(std::make_unique<Resources>())
{
    if (!m_options) {
        m_options = std::make_shared<Options>();
    }
}

BloomFeature::~BloomFeature() = default;

RenderFeatureContract BloomFeature::contract() const noexcept
{
    return RenderFeatureContract{
        .name = kFeatureName,
        .display_name = "Bloom",
        .category = "Post Processing",
        .runtime_toggleable = true,
        .requirements =
            RenderFeatureRequirements{
                .scene_inputs = RenderFeatureSceneInputFlags::SceneColor,
                .resources = RenderFeatureResourceFlags::GraphicsPipeline | RenderFeatureResourceFlags::SampledTexture |
                             RenderFeatureResourceFlags::ColorAttachment | RenderFeatureResourceFlags::UniformBuffer |
                             RenderFeatureResourceFlags::Sampler,
                .rhi_capabilities = RenderFeatureRHICapabilityFlags::DefaultRenderFlow,
                .graph_inputs = kGraphInputs,
                .graph_outputs = kGraphOutputs,
                .requires_framebuffer_size = true,
                .uses_persistent_resources = false,
                .uses_history_resources = false,
            },
    };
}

bool BloomFeature::enabled() const noexcept
{
    return m_options ? m_options->enabled : true;
}

std::vector<RenderFeatureParameterInfo> BloomFeature::parameters() const
{
    const Options options = m_options ? *m_options : Options{};
    return {
        makeFloatParameter("threshold", "Threshold", options.threshold, 0.0f, 10.0f, 0.01f),
        makeFloatParameter("softKnee", "Soft Knee", options.soft_knee, 0.0f, 1.0f, 0.01f),
        makeFloatParameter("intensity", "Intensity", options.intensity, 0.0f, 2.0f, 0.01f),
        makeFloatParameter("radius", "Radius", options.radius, 0.05f, 4.0f, 0.01f),
        makeIntParameter("mipCount", "Mip Count", options.mip_count, 1, static_cast<int32_t>(kMaxBloomMipCount)),
    };
}

RenderFeatureDiagnostics BloomFeature::diagnostics() const
{
    return m_resources ? m_resources->diagnostics() : RenderFeatureDiagnostics{};
}

bool BloomFeature::setEnabled(bool enabled) noexcept
{
    if (!m_options) {
        return false;
    }

    m_options->enabled = enabled;
    return true;
}

bool BloomFeature::setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept
{
    if (!m_options) {
        return false;
    }

    if (name == "threshold" && value.type == RenderFeatureParameterType::Float) {
        m_options->threshold = std::clamp(value.float_value, 0.0f, 10.0f);
        return true;
    }
    if (name == "softKnee" && value.type == RenderFeatureParameterType::Float) {
        m_options->soft_knee = std::clamp(value.float_value, 0.0f, 1.0f);
        return true;
    }
    if (name == "intensity" && value.type == RenderFeatureParameterType::Float) {
        m_options->intensity = std::clamp(value.float_value, 0.0f, 2.0f);
        return true;
    }
    if (name == "radius" && value.type == RenderFeatureParameterType::Float) {
        m_options->radius = std::clamp(value.float_value, 0.05f, 4.0f);
        return true;
    }
    if (name == "mipCount" && value.type == RenderFeatureParameterType::Int) {
        m_options->mip_count = std::clamp(value.int_value, 1, static_cast<int32_t>(kMaxBloomMipCount));
        return true;
    }

    return false;
}

BloomFeature::Options& BloomFeature::options() noexcept
{
    return *m_options;
}

const BloomFeature::Options& BloomFeature::options() const noexcept
{
    return *m_options;
}

bool BloomFeature::registerPasses(RenderFlowBuilder& builder)
{
    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered = builder.insertFeaturePassBetween(kFeatureName,
                                                             extension_slots::AfterTransparent,
                                                             extension_slots::BeforePostProcess,
                                                             "Bloom",
                                                             std::make_unique<BloomPass>(*m_resources, m_options));
    if (registered) {
        LUNA_RENDERER_INFO("Registered Bloom between '{}' and '{}'",
                           extension_slots::AfterTransparent,
                           extension_slots::BeforePostProcess);
    }
    return registered;
}

void BloomFeature::prepareFrame(const RenderWorld& world,
                                const SceneRenderContext& scene_context,
                                const RenderFeatureFrameContext& frame_context,
                                RenderPassBlackboard& blackboard)
{
    (void) world;
    (void) scene_context;
    (void) frame_context;
    (void) blackboard;
}

void BloomFeature::shutdown()
{
    if (m_resources) {
        m_resources->shutdown();
    }
}

} // namespace luna::render_flow
