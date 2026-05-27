#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/Features/FXAAFeature.h"
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

LUNA_REGISTER_RENDER_FEATURE_EX(FXAAFeature, "FXAA", "FXAA", "Anti-Aliasing", kDefaultRenderFlowName, true, true, 40)

void linkFXAAFeature() {}

namespace {

inline constexpr std::string_view kFeatureName = "FXAA";
inline constexpr std::string_view kFXAAColorName = "Scene.FXAA.Color";
inline constexpr RenderResourceKey<RenderGraphTextureHandle> kFXAAColor{kFXAAColorName};

constexpr std::array<RenderFeatureGraphResource, 1> kGraphInputs{{
    {.name = blackboard::SceneFinalColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
}};

constexpr std::array<RenderFeatureGraphResource, 1> kGraphOutputs{{
    {.name = blackboard::SceneFinalColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
}};

constexpr std::array<RenderPassResourceUsage, 1> kPassResources{{
    {.name = blackboard::SceneFinalColor.value(),
     .access = RenderPassResourceAccess::ReadWrite,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

struct FXAAGpuParams {
    glm::vec4 framebuffer;
    glm::vec4 settings;
};

namespace FXAABinding {
constexpr uint32_t SourceColor = 0;
constexpr uint32_t SourceSampler = 1;
constexpr uint32_t Params = 2;
} // namespace FXAABinding

constexpr std::array<RenderFeatureDescriptorBinding, 3> kFXAABindings{{
    {"SourceColor",
     "gSourceColorTexture",
     FXAABinding::SourceColor,
     RHI::DescriptorType::SampledImage,
     1,
     RHI::ShaderStage::Fragment},
    {"SourceSampler",
     "gSourceColorSampler",
     FXAABinding::SourceSampler,
     RHI::DescriptorType::Sampler,
     1,
     RHI::ShaderStage::Fragment},
    {"Params", "gFXAAParams", FXAABinding::Params, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Fragment},
}};

bool isValidTextureHandle(const std::optional<RenderGraphTextureHandle>& handle)
{
    return handle.has_value() && handle->isValid();
}

std::filesystem::path shaderPath()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders" / "FXAA.slang";
}

RHI::DescriptorSetLayoutCreateInfo makeFXAADescriptorSetLayoutCreateInfo()
{
    return makeRenderFeatureDescriptorSetLayoutCreateInfo(kFXAABindings);
}

ShaderBindingContract makeFXAAShaderBindingContract()
{
    return makeRenderFeatureShaderBindingContract(RenderFeatureDescriptorSetContract{
        .contract_name = kFeatureName,
        .set_name = kFeatureName,
        .logical_set = 0,
        .set = 0,
        .bindings = kFXAABindings,
    });
}

RHI::Ref<RHI::DescriptorSetLayout> createDescriptorSetLayout(const RHI::Ref<RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(makeFXAADescriptorSetLayoutCreateInfo());
}

RHI::Ref<RHI::DescriptorPool> createDescriptorPool(const RHI::Ref<RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorPool(RHI::DescriptorPoolBuilder()
                                            .SetMaxSets(2)
                                            .AddPoolSize(RHI::DescriptorType::SampledImage, 2)
                                            .AddPoolSize(RHI::DescriptorType::Sampler, 2)
                                            .AddPoolSize(RHI::DescriptorType::UniformBuffer, 2)
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
                                     .SetName("FXAASampler")
                                     .Build());
}

RHI::Ref<RHI::PipelineLayout> createPipelineLayout(const RHI::Ref<RHI::Device>& device,
                                                   const RHI::Ref<RHI::DescriptorSetLayout>& layout)
{
    if (!device || !layout) {
        return {};
    }

    return device->CreatePipelineLayout(RHI::PipelineLayoutBuilder().AddSetLayout(layout).Build());
}

RHI::Ref<RHI::GraphicsPipeline> createPipeline(const RHI::Ref<RHI::Device>& device,
                                               const RHI::Ref<RHI::PipelineLayout>& layout,
                                               const RHI::Ref<RHI::ShaderModule>& vertex_shader,
                                               const RHI::Ref<RHI::ShaderModule>& fragment_shader,
                                               RHI::Format color_format)
{
    if (!device || !layout || !vertex_shader || !fragment_shader || color_format == RHI::Format::UNDEFINED) {
        return {};
    }

    return device->CreateGraphicsPipeline(RHI::GraphicsPipelineBuilder()
                                              .SetShaders({vertex_shader, fragment_shader})
                                              .SetTopology(RHI::PrimitiveTopology::TriangleList)
                                              .SetCullMode(RHI::CullMode::None)
                                              .SetFrontFace(RHI::FrontFace::CounterClockwise)
                                              .SetDepthTest(false, false, RHI::CompareOp::Always)
                                              .AddColorAttachmentDefault(false)
                                              .AddColorFormat(color_format)
                                              .SetLayout(layout)
                                              .Build());
}

RenderGraphTextureDesc makeFXAAColorDesc(const SceneRenderContext& scene_context)
{
    return RenderGraphTextureDesc{
        .Name = "SceneFXAAColor",
        .Type = RHI::TextureType::Texture2D,
        .Width = scene_context.framebuffer_width,
        .Height = scene_context.framebuffer_height,
        .Depth = 1,
        .ArrayLayers = 1,
        .MipLevels = 1,
        .Format = scene_context.color_format,
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

void clearFXAAPass(RenderGraphRasterPassContext& pass_context)
{
    pass_context.beginRendering();
    pass_context.endRendering();
}

} // namespace

class FXAAFeature::Resources final {
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
            m_resource_set.prepareGpuResourceBuild(context, isCompleteFor(context));
        if (decision.action == RenderFeatureGpuResourceAction::InvalidContext) {
            return false;
        }
        if (decision.action == RenderFeatureGpuResourceAction::Reuse) {
            return true;
        }

        releasePipelineResources();
        const RHI::Ref<RHI::Device>& device = m_resource_set.device();

        const std::filesystem::path path = shaderPath();
        m_state.vertex_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "FXAAVertexMain", RHI::ShaderStage::Vertex);
        m_state.fragment_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "FXAAFragmentMain", RHI::ShaderStage::Fragment);
        m_state.copy_fragment_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "FXAACopyFragmentMain", RHI::ShaderStage::Fragment);

        const ShaderBindingContract contract = makeFXAAShaderBindingContract();
        const std::array<RenderFeatureShaderBindingCheck, 3> binding_checks{{
            {.shader = m_state.vertex_shader, .entry_point = "FXAAVertexMain"},
            {.shader = m_state.fragment_shader, .entry_point = "FXAAFragmentMain"},
            {.shader = m_state.copy_fragment_shader, .entry_point = "FXAACopyFragmentMain"},
        }};
        m_resource_set.validateShaderBindingContract(binding_checks, contract, path);

        m_state.layout = createDescriptorSetLayout(device);
        m_state.descriptor_pool = createDescriptorPool(device);
        m_state.pipeline_layout = createPipelineLayout(device, m_state.layout);
        m_state.pipeline = createPipeline(
            device, m_state.pipeline_layout, m_state.vertex_shader, m_state.fragment_shader, context.color_format);
        m_state.copy_pipeline = createPipeline(
            device, m_state.pipeline_layout, m_state.vertex_shader, m_state.copy_fragment_shader, context.color_format);
        m_state.sampler = createSampler(device);
        m_state.params_buffer = device->CreateBuffer(RHI::BufferBuilder()
                                                         .SetSize(sizeof(FXAAGpuParams))
                                                         .SetUsage(RHI::BufferUsageFlags::UniformBuffer)
                                                         .SetMemoryUsage(RHI::BufferMemoryUsage::CpuToGpu)
                                                         .SetName("FXAAParams")
                                                         .Build());
        m_state.copy_params_buffer = device->CreateBuffer(RHI::BufferBuilder()
                                                              .SetSize(sizeof(FXAAGpuParams))
                                                              .SetUsage(RHI::BufferUsageFlags::UniformBuffer)
                                                              .SetMemoryUsage(RHI::BufferMemoryUsage::CpuToGpu)
                                                              .SetName("FXAACopyParams")
                                                              .Build());

        if (m_state.descriptor_pool && m_state.layout) {
            m_state.descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.layout);
            m_state.copy_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.layout);
        }
        m_state.color_format = context.color_format;

        return m_resource_set.logGpuResourceBuildResult(resourceStatus());
    }

    [[nodiscard]] bool isComplete() const noexcept
    {
        return m_resource_set.hasGpuContext() && m_state.vertex_shader && m_state.fragment_shader &&
               m_state.copy_fragment_shader && m_state.layout && m_state.descriptor_pool && m_state.pipeline_layout &&
               m_state.pipeline && m_state.copy_pipeline && m_state.sampler && m_state.params_buffer &&
               m_state.copy_params_buffer && m_state.descriptor_set && m_state.copy_descriptor_set &&
               m_state.color_format != RHI::Format::UNDEFINED;
    }

    [[nodiscard]] bool isCompleteFor(const SceneRenderContext& context) const noexcept
    {
        return isComplete() && m_state.color_format == context.color_format;
    }

    void updateBindings(const Options& options,
                        const RHI::Ref<RHI::Texture>& source_color,
                        uint32_t width,
                        uint32_t height)
    {
        updateDescriptorSet(m_state.descriptor_set, m_state.params_buffer, options, source_color, width, height);
    }

    void updateCopyBindings(const RHI::Ref<RHI::Texture>& source_color, uint32_t width, uint32_t height)
    {
        updateDescriptorSet(
            m_state.copy_descriptor_set, m_state.copy_params_buffer, Options{}, source_color, width, height);
    }

    void draw(RenderGraphRasterPassContext& pass_context) const
    {
        drawWithPipeline(pass_context, m_state.pipeline, m_state.descriptor_set);
    }

    void drawCopy(RenderGraphRasterPassContext& pass_context) const
    {
        drawWithPipeline(pass_context, m_state.copy_pipeline, m_state.copy_descriptor_set);
    }

    [[nodiscard]] RenderFeatureDiagnostics diagnostics() const
    {
        RenderFeatureDiagnostics result;
        m_resource_set.writeBindingContractDiagnostics(result);
        m_resource_set.writePipelineResourceDiagnostics(result, isComplete(), resourceStatus());
        return result;
    }

private:
    void updateDescriptorSet(const RHI::Ref<RHI::DescriptorSet>& descriptor_set,
                             const RHI::Ref<RHI::Buffer>& params_buffer,
                             const Options& options,
                             const RHI::Ref<RHI::Texture>& source_color,
                             uint32_t width,
                             uint32_t height)
    {
        if (!isComplete() || !descriptor_set || !params_buffer || !source_color || width == 0 || height == 0) {
            return;
        }

        updateParams(params_buffer, options, width, height);
        descriptor_set->WriteTexture(RHI::TextureWriteInfo{
            .Binding = FXAABinding::SourceColor,
            .TextureView = source_color->GetDefaultView(),
            .Layout = RHI::ResourceState::ShaderRead,
            .Type = RHI::DescriptorType::SampledImage,
        });
        descriptor_set->WriteSampler(RHI::SamplerWriteInfo{
            .Binding = FXAABinding::SourceSampler,
            .Sampler = m_state.sampler,
        });
        descriptor_set->WriteBuffer(RHI::BufferWriteInfo{
            .Binding = FXAABinding::Params,
            .Buffer = params_buffer,
            .Offset = 0,
            .Stride = sizeof(FXAAGpuParams),
            .Size = sizeof(FXAAGpuParams),
            .Type = RHI::DescriptorType::UniformBuffer,
        });
        descriptor_set->Update();
    }

    void updateParams(const RHI::Ref<RHI::Buffer>& params_buffer,
                      const Options& options,
                      uint32_t width,
                      uint32_t height)
    {
        if (!params_buffer || width == 0 || height == 0) {
            return;
        }

        const FXAAGpuParams params{
            .framebuffer = glm::vec4(1.0f / static_cast<float>(width),
                                     1.0f / static_cast<float>(height),
                                     static_cast<float>(width),
                                     static_cast<float>(height)),
            .settings = glm::vec4(std::clamp(options.subpixel_quality, 0.0f, 1.0f),
                                  std::clamp(options.edge_threshold, 0.0312f, 0.333f),
                                  std::clamp(options.edge_threshold_min, 0.0f, 0.125f),
                                  0.0f),
        };
        if (void* mapped = params_buffer->Map()) {
            std::memcpy(mapped, &params, sizeof(params));
            params_buffer->Flush();
            params_buffer->Unmap();
        }
    }

    void drawWithPipeline(RenderGraphRasterPassContext& pass_context,
                          const RHI::Ref<RHI::GraphicsPipeline>& pipeline,
                          const RHI::Ref<RHI::DescriptorSet>& descriptor_set) const
    {
        if (!isComplete() || !pipeline || !descriptor_set) {
            clearFXAAPass(pass_context);
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
        const std::array<RHI::Ref<RHI::DescriptorSet>, 1> descriptor_sets{descriptor_set};
        commands.BindDescriptorSets(pipeline, 0, descriptor_sets);
        commands.Draw(3, 1, 0, 0);
        pass_context.endRendering();
    }

    struct State {
        RHI::Ref<RHI::DescriptorSetLayout> layout;
        RHI::Ref<RHI::DescriptorPool> descriptor_pool;
        RHI::Ref<RHI::PipelineLayout> pipeline_layout;
        RHI::Ref<RHI::GraphicsPipeline> pipeline;
        RHI::Ref<RHI::GraphicsPipeline> copy_pipeline;
        RHI::Ref<RHI::Sampler> sampler;
        RHI::Ref<RHI::Buffer> params_buffer;
        RHI::Ref<RHI::Buffer> copy_params_buffer;
        RHI::Ref<RHI::DescriptorSet> descriptor_set;
        RHI::Ref<RHI::DescriptorSet> copy_descriptor_set;
        RHI::Ref<RHI::ShaderModule> vertex_shader;
        RHI::Ref<RHI::ShaderModule> fragment_shader;
        RHI::Ref<RHI::ShaderModule> copy_fragment_shader;
        RHI::Format color_format{RHI::Format::UNDEFINED};
    };

    [[nodiscard]] std::array<RenderFeatureResourceStatus, 13> resourceStatus() const noexcept
    {
        return {{
            {"vertex_shader", static_cast<bool>(m_state.vertex_shader)},
            {"fragment_shader", static_cast<bool>(m_state.fragment_shader)},
            {"copy_fragment_shader", static_cast<bool>(m_state.copy_fragment_shader)},
            {"layout", static_cast<bool>(m_state.layout)},
            {"descriptor_pool", static_cast<bool>(m_state.descriptor_pool)},
            {"pipeline_layout", static_cast<bool>(m_state.pipeline_layout)},
            {"pipeline", static_cast<bool>(m_state.pipeline)},
            {"copy_pipeline", static_cast<bool>(m_state.copy_pipeline)},
            {"sampler", static_cast<bool>(m_state.sampler)},
            {"params_buffer", static_cast<bool>(m_state.params_buffer)},
            {"copy_params_buffer", static_cast<bool>(m_state.copy_params_buffer)},
            {"descriptor_set", static_cast<bool>(m_state.descriptor_set)},
            {"copy_descriptor_set", static_cast<bool>(m_state.copy_descriptor_set)},
        }};
    }

    State m_state{};
    RenderFeatureResourceSet m_resource_set{std::string(kFeatureName)};
};

namespace {

class FXAAPass final : public IRenderPass {
public:
    FXAAPass(FXAAFeature::Resources& resources, FXAAFeature::OptionsHandle options)
        : m_resources(&resources),
          m_options(std::move(options))
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "FXAA";
    }

    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override
    {
        return kPassResources;
    }

    void setup(RenderPassContext& context) override
    {
        const FXAAFeature::Options options = currentOptions();
        if (!options.enabled) {
            return;
        }

        const SceneRenderContext& scene_context = context.sceneContext();
        const auto scene_color = context.blackboard().get(blackboard::SceneFinalColor);
        if (!isValidTextureHandle(scene_color)) {
            LUNA_RENDERER_WARN("FXAA missing input texture '{}'", blackboard::SceneFinalColor.value());
            return;
        }

        const bool resources_ready = m_resources != nullptr && m_resources->ensure(scene_context);
        if (!resources_ready) {
            return;
        }

        const RenderGraphTextureHandle resolved_color = context.graph().CreateTexture(makeFXAAColorDesc(scene_context));
        if (!resolved_color.isValid()) {
            return;
        }

        context.blackboard().set(kFXAAColor, resolved_color);
        blackboard::publishSceneColorStage(context.blackboard(), blackboard::SceneColorStage::Final, *scene_color);

        context.graph().AddRasterPass(
            "FXAAResolve",
            [source = *scene_color, resolved_color](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(source);
                pass_builder.WriteColor(resolved_color,
                                        RHI::AttachmentLoadOp::Clear,
                                        RHI::AttachmentStoreOp::Store,
                                        RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, source = *scene_color, options](RenderGraphRasterPassContext& pass_context) {
                const auto& source_texture = pass_context.getTexture(source);
                if (!source_texture || m_resources == nullptr) {
                    clearFXAAPass(pass_context);
                    return;
                }
                m_resources->updateBindings(
                    options, source_texture, pass_context.framebufferWidth(), pass_context.framebufferHeight());
                m_resources->draw(pass_context);
            });

        context.graph().AddRasterPass(
            "FXAAComposite",
            [resolved_color, destination = *scene_color](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(resolved_color);
                pass_builder.WriteColor(destination,
                                        RHI::AttachmentLoadOp::Clear,
                                        RHI::AttachmentStoreOp::Store,
                                        RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, resolved_color](RenderGraphRasterPassContext& pass_context) {
                const auto& resolved_texture = pass_context.getTexture(resolved_color);
                if (!resolved_texture || m_resources == nullptr) {
                    clearFXAAPass(pass_context);
                    return;
                }
                m_resources->updateCopyBindings(
                    resolved_texture, pass_context.framebufferWidth(), pass_context.framebufferHeight());
                m_resources->drawCopy(pass_context);
            });
    }

private:
    [[nodiscard]] FXAAFeature::Options currentOptions() const
    {
        return m_options ? *m_options : FXAAFeature::Options{};
    }

    FXAAFeature::Resources* m_resources{nullptr};
    FXAAFeature::OptionsHandle m_options;
};

} // namespace

FXAAFeature::FXAAFeature()
    : FXAAFeature(std::make_shared<Options>())
{}

FXAAFeature::FXAAFeature(Options options)
    : FXAAFeature(std::make_shared<Options>(options))
{}

FXAAFeature::FXAAFeature(OptionsHandle options)
    : m_options(std::move(options)),
      m_resources(std::make_unique<Resources>())
{
    if (!m_options) {
        m_options = std::make_shared<Options>();
    }
}

FXAAFeature::~FXAAFeature() = default;

RenderFeatureContract FXAAFeature::contract() const noexcept
{
    return RenderFeatureContract{
        .name = kFeatureName,
        .display_name = "FXAA",
        .category = "Anti-Aliasing",
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
                .uses_temporal_jitter = false,
            },
    };
}

bool FXAAFeature::enabled() const noexcept
{
    return m_options ? m_options->enabled : true;
}

std::vector<RenderFeatureParameterInfo> FXAAFeature::parameters() const
{
    const Options options = m_options ? *m_options : Options{};
    return {
        makeFloatParameter("subpixelQuality", "Subpixel Quality", options.subpixel_quality, 0.0f, 1.0f, 0.01f),
        makeFloatParameter("edgeThreshold", "Edge Threshold", options.edge_threshold, 0.0312f, 0.333f, 0.001f),
        makeFloatParameter("edgeThresholdMin", "Edge Threshold Min", options.edge_threshold_min, 0.0f, 0.125f, 0.001f),
    };
}

RenderFeatureDiagnostics FXAAFeature::diagnostics() const
{
    return m_resources ? m_resources->diagnostics() : RenderFeatureDiagnostics{};
}

bool FXAAFeature::setEnabled(bool enabled) noexcept
{
    if (!m_options) {
        return false;
    }

    m_options->enabled = enabled;
    return true;
}

bool FXAAFeature::setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept
{
    if (!m_options || value.type != RenderFeatureParameterType::Float) {
        return false;
    }

    if (name == "subpixelQuality") {
        m_options->subpixel_quality = std::clamp(value.float_value, 0.0f, 1.0f);
        return true;
    }
    if (name == "edgeThreshold") {
        m_options->edge_threshold = std::clamp(value.float_value, 0.0312f, 0.333f);
        return true;
    }
    if (name == "edgeThresholdMin") {
        m_options->edge_threshold_min = std::clamp(value.float_value, 0.0f, 0.125f);
        return true;
    }

    return false;
}

FXAAFeature::Options& FXAAFeature::options() noexcept
{
    return *m_options;
}

const FXAAFeature::Options& FXAAFeature::options() const noexcept
{
    return *m_options;
}

bool FXAAFeature::registerPasses(RenderFlowBuilder& builder)
{
    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered = builder.insertFeaturePassBetween(kFeatureName,
                                                             extension_slots::AfterPostProcess,
                                                             extension_slots::BeforeOverlay,
                                                             "FXAA",
                                                             std::make_unique<FXAAPass>(*m_resources, m_options));
    if (registered) {
        LUNA_RENDERER_INFO(
            "Registered FXAA between '{}' and '{}'", extension_slots::AfterPostProcess, extension_slots::BeforeOverlay);
    }
    return registered;
}

void FXAAFeature::prepareFrame(const RenderWorld& world,
                               const SceneRenderContext& scene_context,
                               const RenderFeatureFrameContext& frame_context,
                               RenderPassBlackboard& blackboard)
{
    (void) world;
    (void) scene_context;
    (void) frame_context;
    (void) blackboard;
}

void FXAAFeature::shutdown()
{
    if (m_resources) {
        m_resources->shutdown();
    }
}

} // namespace luna::render_flow
