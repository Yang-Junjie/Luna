#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/Features/TemporalAntiAliasingFeature.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFeatureBindingContract.h"
#include "Renderer/RenderFlow/RenderFeatureRegistry.h"
#include "Renderer/RenderFlow/RenderFeatureResources.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderResourceKey.h"
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
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <string>
#include <string_view>
#include <Texture.h>
#include <utility>

namespace luna::render_flow {

LUNA_REGISTER_RENDER_FEATURE_EX(TemporalAntiAliasingFeature,
                                "TemporalAntiAliasing",
                                "Temporal Anti-Aliasing",
                                "Anti-Aliasing",
                                kDefaultRenderFlowName,
                                true,
                                false,
                                20)

void linkTemporalAntiAliasingFeature() {}

namespace {

inline constexpr std::string_view kFeatureName = "TemporalAntiAliasing";
inline constexpr std::string_view kTemporalColorName = "Scene.TemporalAA.Color";
inline constexpr std::string_view kHistoryReadName = "Scene.TemporalAA.HistoryRead";
inline constexpr std::string_view kHistoryWriteName = "Scene.TemporalAA.HistoryWrite";
inline constexpr RenderResourceKey<RenderGraphTextureHandle> kTemporalColor{kTemporalColorName};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> kHistoryRead{kHistoryReadName};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> kHistoryWrite{kHistoryWriteName};
constexpr RenderFeatureGraphResourceFlags kOptionalExternalGraphResourceFlags =
    static_cast<RenderFeatureGraphResourceFlags>(static_cast<uint32_t>(RenderFeatureGraphResourceFlags::Optional) |
                                                 static_cast<uint32_t>(RenderFeatureGraphResourceFlags::External));

constexpr std::array<RenderFeatureGraphResource, 4> kGraphInputs{{
    {blackboard::SceneSkyCompositedColor.value()},
    {blackboard::Depth.value()},
    {blackboard::Velocity.value()},
    {.name = kHistoryReadName, .flags = kOptionalExternalGraphResourceFlags},
}};

constexpr std::array<RenderFeatureGraphResource, 3> kGraphOutputs{{
    {.name = kTemporalColorName, .flags = RenderFeatureGraphResourceFlags::External},
    {.name = kHistoryWriteName, .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneTemporalResolvedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
}};

constexpr std::array<RenderPassResourceUsage, 7> kPassResources{{
    {.name = blackboard::SceneSkyCompositedColor.value(),
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::Depth.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::Velocity.value(), .access = RenderPassResourceAccess::Read},
    {.name = kHistoryReadName, .access = RenderPassResourceAccess::Read, .flags = kOptionalExternalGraphResourceFlags},
    {.name = kTemporalColorName,
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneTemporalResolvedColor.value(),
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = kHistoryWriteName,
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

struct TaaGpuParams {
    glm::vec4 framebuffer;
    glm::vec4 settings;
    glm::vec4 jitter_pixels;
};

namespace taa_binding {
constexpr uint32_t CurrentColor = 0;
constexpr uint32_t HistoryColor = 1;
constexpr uint32_t Velocity = 2;
constexpr uint32_t Depth = 3;
constexpr uint32_t Sampler = 4;
constexpr uint32_t Params = 5;
} // namespace taa_binding

constexpr std::array<RenderFeatureDescriptorBinding, 6> kTaaBindings{{
    {"CurrentColor",
     "gCurrentColorTexture",
     taa_binding::CurrentColor,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"HistoryColor",
     "gHistoryColorTexture",
     taa_binding::HistoryColor,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"Velocity",
     "gVelocityTexture",
     taa_binding::Velocity,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"Depth",
     "gDepthTexture",
     taa_binding::Depth,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"Sampler",
     "gTaaSampler",
     taa_binding::Sampler,
     luna::RHI::DescriptorType::Sampler,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"Params",
     "gTaaParams",
     taa_binding::Params,
     luna::RHI::DescriptorType::UniformBuffer,
     1,
     luna::RHI::ShaderStage::Fragment},
}};

bool isValidTextureHandle(const std::optional<RenderGraphTextureHandle>& handle)
{
    return handle.has_value() && handle->isValid();
}

std::filesystem::path shaderPath()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders" / "TemporalAntiAliasing.slang";
}

luna::RHI::DescriptorSetLayoutCreateInfo makeTaaDescriptorSetLayoutCreateInfo()
{
    return makeRenderFeatureDescriptorSetLayoutCreateInfo(kTaaBindings);
}

ShaderBindingContract makeTaaShaderBindingContract()
{
    return makeRenderFeatureShaderBindingContract(RenderFeatureDescriptorSetContract{
        .contract_name = kFeatureName,
        .set_name = kFeatureName,
        .logical_set = 0,
        .set = 0,
        .bindings = kTaaBindings,
    });
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout>
    createDescriptorSetLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(makeTaaDescriptorSetLayoutCreateInfo());
}

luna::RHI::Ref<luna::RHI::DescriptorPool> createDescriptorPool(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                            .SetMaxSets(8)
                                            .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 24)
                                            .AddPoolSize(luna::RHI::DescriptorType::Sampler, 8)
                                            .AddPoolSize(luna::RHI::DescriptorType::UniformBuffer, 8)
                                            .Build());
}

luna::RHI::Ref<luna::RHI::Sampler> createSampler(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateSampler(luna::RHI::SamplerBuilder()
                                     .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Nearest)
                                     .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToEdge)
                                     .SetAnisotropy(false)
                                     .SetName("TemporalAntiAliasingSampler")
                                     .Build());
}

luna::RHI::Ref<luna::RHI::PipelineLayout>
    createPipelineLayout(const luna::RHI::Ref<luna::RHI::Device>& device,
                         const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& layout)
{
    if (!device || !layout) {
        return {};
    }

    return device->CreatePipelineLayout(luna::RHI::PipelineLayoutBuilder().AddSetLayout(layout).Build());
}

luna::RHI::Ref<luna::RHI::GraphicsPipeline>
    createPipeline(const luna::RHI::Ref<luna::RHI::Device>& device,
                   const luna::RHI::Ref<luna::RHI::PipelineLayout>& layout,
                   const luna::RHI::Ref<luna::RHI::ShaderModule>& vertex_shader,
                   const luna::RHI::Ref<luna::RHI::ShaderModule>& fragment_shader,
                   luna::RHI::Format color_format,
                   uint32_t color_attachment_count)
{
    if (!device || !layout || !vertex_shader || !fragment_shader || color_format == luna::RHI::Format::UNDEFINED ||
        color_attachment_count == 0) {
        return {};
    }

    luna::RHI::GraphicsPipelineBuilder builder;
    builder.SetShaders({vertex_shader, fragment_shader})
        .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
        .SetCullMode(luna::RHI::CullMode::None)
        .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
        .SetDepthTest(false, false, luna::RHI::CompareOp::Always)
        .SetLayout(layout);
    for (uint32_t index = 0; index < color_attachment_count; ++index) {
        builder.AddColorAttachmentDefault(false).AddColorFormat(color_format);
    }
    return device->CreateGraphicsPipeline(builder.Build());
}

RenderGraphTextureDesc makeTemporalColorDesc(const SceneRenderContext& scene_context)
{
    return RenderGraphTextureDesc{
        .Name = "TemporalAntiAliasingColor",
        .Type = luna::RHI::TextureType::Texture2D,
        .Width = scene_context.framebuffer_width,
        .Height = scene_context.framebuffer_height,
        .Depth = 1,
        .ArrayLayers = 1,
        .MipLevels = 1,
        .Format = scene_context.color_format,
        .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .InitialState = luna::RHI::ResourceState::Undefined,
        .SampleCount = luna::RHI::SampleCount::Count1,
    };
}

PersistentTexture2DDesc makeTemporalHistoryDesc(const SceneRenderContext& scene_context)
{
    return PersistentTexture2DDesc{
        .width = scene_context.framebuffer_width,
        .height = scene_context.framebuffer_height,
        .format = scene_context.color_format,
        .usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .initial_state = luna::RHI::ResourceState::Undefined,
        .sample_count = luna::RHI::SampleCount::Count1,
        .name = "TemporalAntiAliasingColor",
    };
}

} // namespace

class TemporalAntiAliasingFeature::Resources final {
public:
    void shutdown()
    {
        releasePersistentResources();
        releasePipelineResources();
        m_resource_set.resetGpuContext();
        m_resource_set.resetBindingContractDiagnostics();
    }

    void releasePipelineResources()
    {
        m_state = {};
    }

    void beginFrame(const RenderFeatureFrameContext& frame_context) noexcept
    {
        m_frame_context = frame_context;
        m_resource_set.beginHistoryFrame(m_history, frame_context);
    }

    [[nodiscard]] bool ensurePipeline(const SceneRenderContext& context)
    {
        const RenderFeatureGpuResourceDecision decision = m_resource_set.prepareGpuResourceBuild(context, isComplete());
        if (decision.action == RenderFeatureGpuResourceAction::InvalidContext) {
            return false;
        }
        if (decision.action == RenderFeatureGpuResourceAction::Reuse) {
            return true;
        }

        releasePipelineResources();
        const luna::RHI::Ref<luna::RHI::Device>& device = m_resource_set.device();

        const std::filesystem::path path = shaderPath();
        m_state.vertex_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "taaVertexMain", luna::RHI::ShaderStage::Vertex);
        m_state.fragment_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "taaFragmentMain", luna::RHI::ShaderStage::Fragment);
        m_state.copy_fragment_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "taaCopyFragmentMain", luna::RHI::ShaderStage::Fragment);

        const ShaderBindingContract contract = makeTaaShaderBindingContract();
        const std::array<RenderFeatureShaderBindingCheck, 3> binding_checks{{
            {.shader = m_state.vertex_shader, .entry_point = "taaVertexMain"},
            {.shader = m_state.fragment_shader, .entry_point = "taaFragmentMain"},
            {.shader = m_state.copy_fragment_shader, .entry_point = "taaCopyFragmentMain"},
        }};
        m_resource_set.validateShaderBindingContract(binding_checks, contract, path);

        m_state.layout = createDescriptorSetLayout(device);
        m_state.descriptor_pool = createDescriptorPool(device);
        m_state.pipeline_layout = createPipelineLayout(device, m_state.layout);
        m_state.pipeline = createPipeline(
            device, m_state.pipeline_layout, m_state.vertex_shader, m_state.fragment_shader, context.color_format, 2);
        m_state.copy_pipeline = createPipeline(device,
                                               m_state.pipeline_layout,
                                               m_state.vertex_shader,
                                               m_state.copy_fragment_shader,
                                               context.color_format,
                                               1);
        m_state.sampler = createSampler(device);
        m_state.resolve_params_buffer = device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                 .SetSize(sizeof(TaaGpuParams))
                                                                 .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                                 .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                 .SetName("TemporalAntiAliasingResolveParams")
                                                                 .Build());
        m_state.copy_params_buffer = device->CreateBuffer(luna::RHI::BufferBuilder()
                                                              .SetSize(sizeof(TaaGpuParams))
                                                              .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                              .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                              .SetName("TemporalAntiAliasingCopyParams")
                                                              .Build());

        if (m_state.descriptor_pool && m_state.layout) {
            m_state.resolve_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.layout);
            m_state.copy_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.layout);
        }

        return m_resource_set.logGpuResourceBuildResult(resourceStatus());
    }

    [[nodiscard]] bool ensureHistory(const SceneRenderContext& context)
    {
        m_history_evaluated = true;
        if (!context.device) {
            m_resource_set.resetHistoryTexture2D(m_history);
            return false;
        }

        return m_resource_set.ensureHistoryTexture2D(m_history, context, makeTemporalHistoryDesc(context));
    }

    [[nodiscard]] RenderGraphTextureHandle importHistoryRead(RenderGraphBuilder& graph)
    {
        return m_resource_set.importHistoryReadTexture2D(graph,
                                                         m_history,
                                                         RenderFeatureTextureImportOptions{
                                                             .name = "TemporalAntiAliasingHistoryRead",
                                                             .final_state = luna::RHI::ResourceState::ShaderRead,
                                                             .export_texture = false,
                                                         });
    }

    [[nodiscard]] RenderGraphTextureHandle importHistoryWrite(RenderGraphBuilder& graph)
    {
        return m_resource_set.importHistoryWriteTexture2D(graph,
                                                          m_history,
                                                          RenderFeatureTextureImportOptions{
                                                              .name = "TemporalAntiAliasingHistoryWrite",
                                                              .final_state = luna::RHI::ResourceState::ShaderRead,
                                                          });
    }

    void commitFrame() noexcept
    {
        m_resource_set.commitHistoryTexture2D(m_history);
    }

    void releasePersistentResources() noexcept
    {
        m_resource_set.resetHistoryTexture2D(m_history);
        m_history_evaluated = false;
    }

    [[nodiscard]] bool isComplete() const noexcept
    {
        return m_resource_set.hasGpuContext() && m_state.vertex_shader && m_state.fragment_shader && m_state.layout &&
               m_state.copy_fragment_shader && m_state.descriptor_pool && m_state.pipeline_layout && m_state.pipeline &&
               m_state.copy_pipeline && m_state.sampler && m_state.resolve_params_buffer &&
               m_state.copy_params_buffer && m_state.resolve_descriptor_set && m_state.copy_descriptor_set;
    }

    void updateBindings(const luna::RHI::Ref<luna::RHI::Texture>& current_color,
                        const luna::RHI::Ref<luna::RHI::Texture>& history_color,
                        const luna::RHI::Ref<luna::RHI::Texture>& velocity,
                        const luna::RHI::Ref<luna::RHI::Texture>& depth,
                        uint32_t width,
                        uint32_t height,
                        bool has_readable_history)
    {
        if (!isComplete() || !current_color || !history_color || !velocity || !depth || width == 0 || height == 0) {
            return;
        }

        updateParams(m_state.resolve_params_buffer, width, height, has_readable_history);
        m_state.resolve_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = taa_binding::CurrentColor,
            .TextureView = current_color->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.resolve_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = taa_binding::HistoryColor,
            .TextureView = history_color->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.resolve_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = taa_binding::Velocity,
            .TextureView = velocity->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.resolve_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = taa_binding::Depth,
            .TextureView = depth->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.resolve_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
            .Binding = taa_binding::Sampler,
            .Sampler = m_state.sampler,
        });
        m_state.resolve_descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
            .Binding = taa_binding::Params,
            .Buffer = m_state.resolve_params_buffer,
            .Offset = 0,
            .Stride = sizeof(TaaGpuParams),
            .Size = sizeof(TaaGpuParams),
            .Type = luna::RHI::DescriptorType::UniformBuffer,
        });
        m_state.resolve_descriptor_set->Update();
    }

    void updateCopyBindings(const luna::RHI::Ref<luna::RHI::Texture>& color, uint32_t width, uint32_t height)
    {
        if (!isComplete() || !color || width == 0 || height == 0) {
            return;
        }

        updateParams(m_state.copy_params_buffer, width, height, false);
        m_state.copy_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = taa_binding::CurrentColor,
            .TextureView = color->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.copy_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
            .Binding = taa_binding::Sampler,
            .Sampler = m_state.sampler,
        });
        m_state.copy_descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
            .Binding = taa_binding::Params,
            .Buffer = m_state.copy_params_buffer,
            .Offset = 0,
            .Stride = sizeof(TaaGpuParams),
            .Size = sizeof(TaaGpuParams),
            .Type = luna::RHI::DescriptorType::UniformBuffer,
        });
        m_state.copy_descriptor_set->Update();
    }

    void draw(RenderGraphRasterPassContext& pass_context) const
    {
        if (!isComplete()) {
            pass_context.beginRendering();
            pass_context.endRendering();
            return;
        }

        pass_context.beginRendering();
        auto& commands = pass_context.commandBuffer();
        commands.BindGraphicsPipeline(m_state.pipeline);
        commands.SetViewport({0.0f,
                              0.0f,
                              static_cast<float>(pass_context.framebufferWidth()),
                              static_cast<float>(pass_context.framebufferHeight()),
                              0.0f,
                              1.0f});
        commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{m_state.resolve_descriptor_set};
        commands.BindDescriptorSets(m_state.pipeline, 0, descriptor_sets);
        commands.Draw(3, 1, 0, 0);
        pass_context.endRendering();
    }

    void drawCopy(RenderGraphRasterPassContext& pass_context) const
    {
        if (!isComplete()) {
            pass_context.beginRendering();
            pass_context.endRendering();
            return;
        }

        pass_context.beginRendering();
        auto& commands = pass_context.commandBuffer();
        commands.BindGraphicsPipeline(m_state.copy_pipeline);
        commands.SetViewport({0.0f,
                              0.0f,
                              static_cast<float>(pass_context.framebufferWidth()),
                              static_cast<float>(pass_context.framebufferHeight()),
                              0.0f,
                              1.0f});
        commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{m_state.copy_descriptor_set};
        commands.BindDescriptorSets(m_state.copy_pipeline, 0, descriptor_sets);
        commands.Draw(3, 1, 0, 0);
        pass_context.endRendering();
    }

    [[nodiscard]] RenderFeatureDiagnostics diagnostics() const
    {
        RenderFeatureDiagnostics result;
        m_resource_set.writeBindingContractDiagnostics(result);
        m_resource_set.writePipelineResourceDiagnostics(result, isComplete(), resourceStatus());
        m_resource_set.writeHistoryResourceDiagnostics(result, m_history_evaluated, m_history, historyResourceStatus());
        return result;
    }

private:
    void updateParams(const luna::RHI::Ref<luna::RHI::Buffer>& params_buffer,
                      uint32_t width,
                      uint32_t height,
                      bool has_readable_history)
    {
        if (!params_buffer) {
            return;
        }

        constexpr float kHistoryBlend = 0.95f;
        constexpr float kVarianceClipGamma = 1.50f;
        constexpr float kMotionRejectScale = 0.05f;
        const TaaGpuParams params{
            .framebuffer = glm::vec4(1.0f / static_cast<float>(width),
                                     1.0f / static_cast<float>(height),
                                     static_cast<float>(width),
                                     static_cast<float>(height)),
            .settings = glm::vec4(std::clamp(kHistoryBlend, 0.0f, 0.98f),
                                  has_readable_history ? 1.0f : 0.0f,
                                  kVarianceClipGamma,
                                  kMotionRejectScale),
            .jitter_pixels = glm::vec4(m_frame_context.view.jitter_pixels, m_frame_context.view.previous_jitter_pixels),
        };
        if (void* mapped = params_buffer->Map()) {
            std::memcpy(mapped, &params, sizeof(params));
            params_buffer->Flush();
            params_buffer->Unmap();
        }
    }

    struct State {
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> layout;
        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;
        luna::RHI::Ref<luna::RHI::PipelineLayout> pipeline_layout;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> pipeline;
        luna::RHI::Ref<luna::RHI::Sampler> sampler;
        luna::RHI::Ref<luna::RHI::Buffer> resolve_params_buffer;
        luna::RHI::Ref<luna::RHI::Buffer> copy_params_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> resolve_descriptor_set;
        luna::RHI::Ref<luna::RHI::DescriptorSet> copy_descriptor_set;
        luna::RHI::Ref<luna::RHI::ShaderModule> vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> copy_fragment_shader;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> copy_pipeline;
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
            {"resolve_params_buffer", static_cast<bool>(m_state.resolve_params_buffer)},
            {"copy_params_buffer", static_cast<bool>(m_state.copy_params_buffer)},
            {"resolve_descriptor_set", static_cast<bool>(m_state.resolve_descriptor_set)},
            {"copy_descriptor_set", static_cast<bool>(m_state.copy_descriptor_set)},
        }};
    }

    [[nodiscard]] std::array<RenderFeatureResourceStatus, 3> historyResourceStatus() const noexcept
    {
        return {{
            {"history_texture", m_history.isValid()},
            {"history_read", m_history.readTexture() != nullptr},
            {"history_write", m_history.writeTexture() != nullptr},
        }};
    }

    State m_state{};
    RenderFeatureResourceSet m_resource_set{std::string(kFeatureName)};
    HistoryTexture2D m_history;
    RenderFeatureFrameContext m_frame_context{};
    bool m_history_evaluated{false};
};

namespace {

class TemporalAntiAliasingPass final : public IRenderPass {
public:
    explicit TemporalAntiAliasingPass(TemporalAntiAliasingFeature::Resources& resources)
        : m_resources(&resources)
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "TemporalAntiAliasing";
    }

    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override
    {
        return kPassResources;
    }

    void setup(RenderPassContext& context) override
    {
        const SceneRenderContext& scene_context = context.sceneContext();
        blackboard::publishSceneColorStage(
            context.blackboard(), blackboard::SceneColorStage::TemporalResolved, scene_context.color_target);
        const auto scene_color = context.blackboard().get(blackboard::SceneSkyCompositedColor);
        const auto depth = context.blackboard().get(blackboard::Depth);
        const auto velocity = context.blackboard().get(blackboard::Velocity);
        if (!isValidTextureHandle(scene_color) || !isValidTextureHandle(velocity)) {
            return;
        }

        const RenderGraphTextureHandle temporal_color =
            context.graph().CreateTexture(makeTemporalColorDesc(scene_context));
        if (!temporal_color.isValid()) {
            return;
        }

        RenderGraphTextureHandle history_read;
        RenderGraphTextureHandle history_write;
        const bool pipeline_ready = m_resources != nullptr && m_resources->ensurePipeline(scene_context);
        const bool history_ready = m_resources != nullptr && m_resources->ensureHistory(scene_context);
        if (history_ready) {
            history_read = m_resources->importHistoryRead(context.graph());
        }
        if (pipeline_ready && history_ready) {
            history_write = m_resources->importHistoryWrite(context.graph());
        }
        if (!history_write.isValid()) {
            return;
        }

        context.blackboard().set(kTemporalColor, temporal_color);
        if (history_read.isValid()) {
            context.blackboard().set(kHistoryRead, history_read);
        }
        if (history_write.isValid()) {
            context.blackboard().set(kHistoryWrite, history_write);
        }

        context.graph().AddRasterPass(
            name(),
            [temporal_color, history_read, history_write, scene_color, depth, velocity](
                RenderGraphRasterPassBuilder& pass_builder) {
                if (isValidTextureHandle(scene_color)) {
                    pass_builder.ReadTexture(*scene_color);
                }
                if (isValidTextureHandle(depth)) {
                    pass_builder.ReadTexture(*depth);
                }
                if (isValidTextureHandle(velocity)) {
                    pass_builder.ReadTexture(*velocity);
                }
                if (history_read.isValid()) {
                    pass_builder.ReadTexture(history_read);
                }
                pass_builder.WriteColor(temporal_color,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
                pass_builder.WriteColor(history_write,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, scene_color, history_read, depth, velocity, scene_context](
                RenderGraphRasterPassContext& pass_context) {
                if (!isValidTextureHandle(scene_color) || !isValidTextureHandle(depth) ||
                    !isValidTextureHandle(velocity) || m_resources == nullptr || !m_resources->isComplete()) {
                    pass_context.beginRendering();
                    pass_context.endRendering();
                    return;
                }

                const auto& current_color_texture = pass_context.getTexture(*scene_color);
                const auto& depth_texture = pass_context.getTexture(*depth);
                const auto& velocity_texture = pass_context.getTexture(*velocity);
                const auto& history_texture =
                    history_read.isValid() ? pass_context.getTexture(history_read) : current_color_texture;
                if (!current_color_texture || !depth_texture || !velocity_texture || !history_texture) {
                    pass_context.beginRendering();
                    pass_context.endRendering();
                    return;
                }

                m_resources->updateBindings(current_color_texture,
                                            history_texture,
                                            velocity_texture,
                                            depth_texture,
                                            scene_context.framebuffer_width,
                                            scene_context.framebuffer_height,
                                            history_read.isValid());
                m_resources->draw(pass_context);
            });

        context.graph().AddRasterPass(
            "TemporalAntiAliasingApply",
            [temporal_color, scene_context](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(temporal_color);
                pass_builder.WriteColor(scene_context.color_target,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, temporal_color, scene_context](RenderGraphRasterPassContext& pass_context) {
                if (m_resources == nullptr || !m_resources->isComplete()) {
                    pass_context.beginRendering();
                    pass_context.endRendering();
                    return;
                }

                const auto& temporal_texture = pass_context.getTexture(temporal_color);
                if (!temporal_texture) {
                    pass_context.beginRendering();
                    pass_context.endRendering();
                    return;
                }

                m_resources->updateCopyBindings(
                    temporal_texture, scene_context.framebuffer_width, scene_context.framebuffer_height);
                m_resources->drawCopy(pass_context);
            });
    }

private:
    TemporalAntiAliasingFeature::Resources* m_resources{nullptr};
};

} // namespace

TemporalAntiAliasingFeature::TemporalAntiAliasingFeature()
    : TemporalAntiAliasingFeature(std::make_shared<Options>())
{}

TemporalAntiAliasingFeature::TemporalAntiAliasingFeature(Options options)
    : TemporalAntiAliasingFeature(std::make_shared<Options>(options))
{}

TemporalAntiAliasingFeature::TemporalAntiAliasingFeature(OptionsHandle options)
    : m_options(std::move(options)),
      m_resources(std::make_unique<Resources>())
{
    if (!m_options) {
        m_options = std::make_shared<Options>();
    }
}

TemporalAntiAliasingFeature::~TemporalAntiAliasingFeature() = default;

RenderFeatureContract TemporalAntiAliasingFeature::contract() const noexcept
{
    return RenderFeatureContract{
        .name = kFeatureName,
        .display_name = "Temporal Anti-Aliasing",
        .category = "Anti-Aliasing",
        .runtime_toggleable = true,
        .requirements =
            RenderFeatureRequirements{
                .scene_inputs = RenderFeatureSceneInputFlags::SceneColor | RenderFeatureSceneInputFlags::Depth |
                                RenderFeatureSceneInputFlags::Velocity,
                .resources = RenderFeatureResourceFlags::GraphicsPipeline | RenderFeatureResourceFlags::SampledTexture |
                             RenderFeatureResourceFlags::ColorAttachment | RenderFeatureResourceFlags::UniformBuffer |
                             RenderFeatureResourceFlags::Sampler,
                .rhi_capabilities = RenderFeatureRHICapabilityFlags::DefaultRenderFlow,
                .graph_inputs = kGraphInputs,
                .graph_outputs = kGraphOutputs,
                .requires_framebuffer_size = true,
                .uses_persistent_resources = false,
                .uses_history_resources = true,
                .uses_temporal_jitter = true,
            },
    };
}

bool TemporalAntiAliasingFeature::enabled() const noexcept
{
    return m_options ? m_options->enabled : false;
}

RenderFeatureDiagnostics TemporalAntiAliasingFeature::diagnostics() const
{
    return m_resources ? m_resources->diagnostics() : RenderFeatureDiagnostics{};
}

bool TemporalAntiAliasingFeature::setEnabled(bool enabled) noexcept
{
    if (!m_options) {
        return false;
    }

    m_options->enabled = enabled;
    return true;
}

bool TemporalAntiAliasingFeature::registerPasses(RenderFlowBuilder& builder)
{
    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered = builder.insertFeaturePassBetween(kFeatureName,
                                                             extension_slots::AfterSky,
                                                             extension_slots::BeforeTransparent,
                                                             "TemporalAntiAliasing",
                                                             std::make_unique<TemporalAntiAliasingPass>(*m_resources));
    if (registered) {
        LUNA_RENDERER_INFO("Registered TemporalAntiAliasing between '{}' and '{}'",
                           extension_slots::AfterSky,
                           extension_slots::BeforeTransparent);
    }
    return registered;
}

void TemporalAntiAliasingFeature::prepareFrame(const RenderWorld& world,
                                               const SceneRenderContext& scene_context,
                                               const RenderFeatureFrameContext& frame_context,
                                               RenderPassBlackboard& blackboard)
{
    (void) world;
    (void) scene_context;
    (void) blackboard;
    if (m_resources) {
        m_resources->beginFrame(frame_context);
    }
}

void TemporalAntiAliasingFeature::commitFrame()
{
    if (m_resources) {
        m_resources->commitFrame();
    }
}

void TemporalAntiAliasingFeature::releasePersistentResources()
{
    if (m_resources) {
        m_resources->releasePersistentResources();
    }
}

void TemporalAntiAliasingFeature::shutdown()
{
    if (m_resources) {
        m_resources->shutdown();
    }
}

} // namespace luna::render_flow
