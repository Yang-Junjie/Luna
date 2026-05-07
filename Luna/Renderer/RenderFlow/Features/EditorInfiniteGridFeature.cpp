#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/Features/EditorInfiniteGridFeature.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFeatureBindingContract.h"
#include "Renderer/RenderFlow/RenderFeatureRegistry.h"
#include "Renderer/RenderFlow/RenderFeatureResources.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
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
#include <glm/mat4x4.hpp>
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

LUNA_REGISTER_RENDER_FEATURE_EX(EditorInfiniteGridFeature,
                                "EditorInfiniteGrid",
                                "Editor Infinite Grid",
                                "Editor",
                                kDefaultRenderFlowName,
                                true,
                                false,
                                10)

void linkEditorInfiniteGridFeature() {}

namespace {

inline constexpr std::string_view kFeatureName = "EditorInfiniteGrid";
constexpr std::array<RenderFeatureGraphResource, 3> kGraphInputs{{
    {.name = blackboard::GBufferBaseColor.value()},
    {.name = blackboard::GBufferNormalMetallic.value()},
    {.name = blackboard::GBufferWorldPositionRoughness.value()},
}};

constexpr std::array<RenderFeatureGraphResource, 1> kGraphOutputs{{
    {.name = blackboard::SceneSkyCompositedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
}};

constexpr std::array<RenderPassResourceUsage, 4> kPassResources{{
    {.name = blackboard::SceneSkyCompositedColor.value(),
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::GBufferBaseColor.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::GBufferNormalMetallic.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::GBufferWorldPositionRoughness.value(), .access = RenderPassResourceAccess::Read},
}};

struct GridGpuParams {
    glm::mat4 view_projection{1.0f};
    glm::mat4 inverse_view_projection{1.0f};
    glm::vec4 camera_position_fade_distance{0.0f, 0.0f, 0.0f, 500.0f};
    glm::vec4 framebuffer{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 settings{1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 thin_color{0.50f, 0.50f, 0.50f, 0.40f};
    glm::vec4 thick_color{0.50f, 0.50f, 0.50f, 0.60f};
    glm::vec4 axis_color_x{0.90f, 0.20f, 0.20f, 1.0f};
    glm::vec4 axis_color_z{0.20f, 0.20f, 0.90f, 1.0f};
};

namespace grid_binding {
constexpr uint32_t Params = 0;
constexpr uint32_t GBufferBaseColor = 1;
constexpr uint32_t GBufferNormalMetallic = 2;
constexpr uint32_t GBufferWorldPositionRoughness = 3;
constexpr uint32_t GBufferSampler = 4;
} // namespace grid_binding

constexpr std::array<RenderFeatureDescriptorBinding, 5> kGridBindings{{
    {"Params",
     "gGridParams",
     grid_binding::Params,
     luna::RHI::DescriptorType::UniformBuffer,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"GBufferBaseColor",
     "gGBufferBaseColor",
     grid_binding::GBufferBaseColor,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"GBufferNormalMetallic",
     "gGBufferNormalMetallic",
     grid_binding::GBufferNormalMetallic,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"GBufferWorldPositionRoughness",
     "gGBufferWorldPositionRoughness",
     grid_binding::GBufferWorldPositionRoughness,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"GBufferSampler",
     "gGBufferSampler",
     grid_binding::GBufferSampler,
     luna::RHI::DescriptorType::Sampler,
     1,
     luna::RHI::ShaderStage::Fragment},
}};

bool isValidTextureHandle(const std::optional<RenderGraphTextureHandle>& handle)
{
    return handle.has_value() && handle->isValid();
}

std::filesystem::path shaderPath()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders" / "EditorInfiniteGrid.slang";
}

luna::RHI::ColorBlendAttachmentState makeAlphaBlendAttachment()
{
    luna::RHI::ColorBlendAttachmentState blend_attachment{};
    blend_attachment.BlendEnable = true;
    blend_attachment.SrcColorBlendFactor = luna::RHI::BlendFactor::SrcAlpha;
    blend_attachment.DstColorBlendFactor = luna::RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.ColorBlendOp = luna::RHI::BlendOp::Add;
    blend_attachment.SrcAlphaBlendFactor = luna::RHI::BlendFactor::One;
    blend_attachment.DstAlphaBlendFactor = luna::RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.AlphaBlendOp = luna::RHI::BlendOp::Add;
    blend_attachment.ColorWriteMask = luna::RHI::ColorComponentFlags::All;
    return blend_attachment;
}

luna::RHI::DescriptorSetLayoutCreateInfo makeGridDescriptorSetLayoutCreateInfo()
{
    return makeRenderFeatureDescriptorSetLayoutCreateInfo(kGridBindings);
}

ShaderBindingContract makeGridShaderBindingContract()
{
    return makeRenderFeatureShaderBindingContract(RenderFeatureDescriptorSetContract{
        .contract_name = kFeatureName,
        .set_name = kFeatureName,
        .logical_set = 0,
        .set = 0,
        .bindings = kGridBindings,
    });
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout>
    createDescriptorSetLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(makeGridDescriptorSetLayoutCreateInfo());
}

luna::RHI::Ref<luna::RHI::DescriptorPool> createDescriptorPool(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                            .SetMaxSets(4)
                                            .AddPoolSize(luna::RHI::DescriptorType::UniformBuffer, 4)
                                            .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 12)
                                            .AddPoolSize(luna::RHI::DescriptorType::Sampler, 4)
                                            .Build());
}

luna::RHI::Ref<luna::RHI::Sampler> createSampler(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateSampler(luna::RHI::SamplerBuilder()
                                     .SetFilter(luna::RHI::Filter::Nearest, luna::RHI::Filter::Nearest)
                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Nearest)
                                     .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToEdge)
                                     .SetAnisotropy(false)
                                     .SetName("EditorInfiniteGridGBufferSampler")
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
                   luna::RHI::Format color_format)
{
    if (!device || !layout || !vertex_shader || !fragment_shader || color_format == luna::RHI::Format::UNDEFINED) {
        return {};
    }

    return device->CreateGraphicsPipeline(luna::RHI::GraphicsPipelineBuilder()
                                              .SetShaders({vertex_shader, fragment_shader})
                                              .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
                                              .SetCullMode(luna::RHI::CullMode::None)
                                              .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
                                              .SetDepthTest(false, false, luna::RHI::CompareOp::Always)
                                              .AddColorAttachment(makeAlphaBlendAttachment())
                                              .AddColorFormat(color_format)
                                              .SetLayout(layout)
                                              .Build());
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

void clearGridPass(RenderGraphRasterPassContext& pass_context)
{
    pass_context.beginRendering();
    pass_context.endRendering();
}

} // namespace

class EditorInfiniteGridFeature::Resources final {
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

    void beginFrame(const RenderFeatureFrameContext& frame_context) noexcept
    {
        m_frame_context = frame_context;
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
        const luna::RHI::Ref<luna::RHI::Device>& device = m_resource_set.device();

        const std::filesystem::path path = shaderPath();
        m_state.vertex_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "editorInfiniteGridVertexMain", luna::RHI::ShaderStage::Vertex);
        m_state.fragment_shader = renderer_detail::loadShaderModule(
            device, context.compiler, path, "editorInfiniteGridFragmentMain", luna::RHI::ShaderStage::Fragment);

        const ShaderBindingContract contract = makeGridShaderBindingContract();
        const std::array<RenderFeatureShaderBindingCheck, 2> binding_checks{{
            {.shader = m_state.vertex_shader, .entry_point = "editorInfiniteGridVertexMain"},
            {.shader = m_state.fragment_shader, .entry_point = "editorInfiniteGridFragmentMain"},
        }};
        m_resource_set.validateShaderBindingContract(binding_checks, contract, path);

        m_state.layout = createDescriptorSetLayout(device);
        m_state.descriptor_pool = createDescriptorPool(device);
        m_state.pipeline_layout = createPipelineLayout(device, m_state.layout);
        m_state.pipeline = createPipeline(
            device, m_state.pipeline_layout, m_state.vertex_shader, m_state.fragment_shader, context.color_format);
        m_state.sampler = createSampler(device);
        m_state.params_buffer = device->CreateBuffer(luna::RHI::BufferBuilder()
                                                         .SetSize(sizeof(GridGpuParams))
                                                         .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                         .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                         .SetName("EditorInfiniteGridParams")
                                                         .Build());
        if (m_state.descriptor_pool && m_state.layout) {
            m_state.descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.layout);
        }
        m_state.color_format = context.color_format;

        return m_resource_set.logGpuResourceBuildResult(resourceStatus());
    }

    [[nodiscard]] bool isComplete() const noexcept
    {
        return m_resource_set.hasGpuContext() && m_state.vertex_shader && m_state.fragment_shader && m_state.layout &&
               m_state.descriptor_pool && m_state.pipeline_layout && m_state.pipeline && m_state.sampler &&
               m_state.params_buffer && m_state.descriptor_set && m_state.color_format != luna::RHI::Format::UNDEFINED;
    }

    [[nodiscard]] bool isCompleteFor(const SceneRenderContext& context) const noexcept
    {
        return isComplete() && m_state.color_format == context.color_format;
    }

    void updateBindings(const Options& options,
                        const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                        const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                        const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                        uint32_t width,
                        uint32_t height)
    {
        if (!isComplete() || !gbuffer_base_color || !gbuffer_normal_metallic || !gbuffer_world_position_roughness) {
            return;
        }

        updateParams(options, width, height);
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = grid_binding::GBufferBaseColor,
            .TextureView = gbuffer_base_color->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = grid_binding::GBufferNormalMetallic,
            .TextureView = gbuffer_normal_metallic->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = grid_binding::GBufferWorldPositionRoughness,
            .TextureView = gbuffer_world_position_roughness->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
            .Binding = grid_binding::GBufferSampler,
            .Sampler = m_state.sampler,
        });
        m_state.descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
            .Binding = grid_binding::Params,
            .Buffer = m_state.params_buffer,
            .Offset = 0,
            .Stride = sizeof(GridGpuParams),
            .Size = sizeof(GridGpuParams),
            .Type = luna::RHI::DescriptorType::UniformBuffer,
        });
        m_state.descriptor_set->Update();
    }

    void draw(RenderGraphRasterPassContext& pass_context) const
    {
        if (!isComplete()) {
            clearGridPass(pass_context);
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
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{m_state.descriptor_set};
        commands.BindDescriptorSets(m_state.pipeline, 0, descriptor_sets);
        commands.Draw(3, 1, 0, 0);
        pass_context.endRendering();
    }

    [[nodiscard]] RenderFeatureDiagnostics diagnostics() const
    {
        RenderFeatureDiagnostics result;
        m_resource_set.writeBindingContractDiagnostics(result);
        m_resource_set.writePipelineResourceDiagnostics(result, isComplete(), resourceStatus());
        return result;
    }

private:
    void updateParams(const Options& options, uint32_t width, uint32_t height)
    {
        if (!m_state.params_buffer) {
            return;
        }

        const float framebuffer_width = static_cast<float>((std::max)(width, 1u));
        const float framebuffer_height = static_cast<float>((std::max)(height, 1u));
        const RenderViewMatrices& matrices = m_frame_context.view.current;
        const glm::vec3 camera_position = glm::vec3(matrices.inverse_view[3]);
        const GridGpuParams params{
            .view_projection = matrices.view_projection,
            .inverse_view_projection = matrices.inverse_view_projection,
            .camera_position_fade_distance =
                glm::vec4(camera_position, std::clamp(options.fade_distance, 1.0f, 10000.0f)),
            .framebuffer =
                glm::vec4(1.0f / framebuffer_width, 1.0f / framebuffer_height, framebuffer_width, framebuffer_height),
            .settings = glm::vec4(
                std::clamp(options.grid_scale, 0.001f, 1000.0f), std::clamp(options.opacity, 0.0f, 1.0f), 0.0f, 0.0f),
            .thin_color = glm::vec4(0.50f, 0.50f, 0.50f, 0.40f),
            .thick_color = glm::vec4(0.50f, 0.50f, 0.50f, 0.60f),
            .axis_color_x = glm::vec4(0.90f, 0.20f, 0.20f, 1.0f),
            .axis_color_z = glm::vec4(0.20f, 0.20f, 0.90f, 1.0f),
        };

        if (void* mapped = m_state.params_buffer->Map()) {
            std::memcpy(mapped, &params, sizeof(params));
            m_state.params_buffer->Flush();
            m_state.params_buffer->Unmap();
        }
    }

    struct State {
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> layout;
        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;
        luna::RHI::Ref<luna::RHI::PipelineLayout> pipeline_layout;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> pipeline;
        luna::RHI::Ref<luna::RHI::Sampler> sampler;
        luna::RHI::Ref<luna::RHI::Buffer> params_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> descriptor_set;
        luna::RHI::Ref<luna::RHI::ShaderModule> vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> fragment_shader;
        luna::RHI::Format color_format{luna::RHI::Format::UNDEFINED};
    };

    [[nodiscard]] std::array<RenderFeatureResourceStatus, 9> resourceStatus() const noexcept
    {
        return {{
            {"vertex_shader", static_cast<bool>(m_state.vertex_shader)},
            {"fragment_shader", static_cast<bool>(m_state.fragment_shader)},
            {"layout", static_cast<bool>(m_state.layout)},
            {"descriptor_pool", static_cast<bool>(m_state.descriptor_pool)},
            {"pipeline_layout", static_cast<bool>(m_state.pipeline_layout)},
            {"pipeline", static_cast<bool>(m_state.pipeline)},
            {"sampler", static_cast<bool>(m_state.sampler)},
            {"params_buffer", static_cast<bool>(m_state.params_buffer)},
            {"descriptor_set", static_cast<bool>(m_state.descriptor_set)},
        }};
    }

    State m_state{};
    RenderFeatureResourceSet m_resource_set{std::string(kFeatureName)};
    RenderFeatureFrameContext m_frame_context{};
};

namespace {

class EditorInfiniteGridPass final : public IRenderPass {
public:
    EditorInfiniteGridPass(EditorInfiniteGridFeature::Resources& resources,
                           EditorInfiniteGridFeature::OptionsHandle options)
        : m_resources(&resources),
          m_options(std::move(options))
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "EditorInfiniteGrid";
    }

    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override
    {
        return kPassResources;
    }

    void setup(RenderPassContext& context) override
    {
        const EditorInfiniteGridFeature::Options options = currentOptions();
        if (!options.enabled) {
            return;
        }

        const SceneRenderContext& scene_context = context.sceneContext();
        const auto scene_color = context.blackboard().get(blackboard::SceneSkyCompositedColor);
        const auto gbuffer_base_color = context.blackboard().get(blackboard::GBufferBaseColor);
        const auto gbuffer_normal_metallic = context.blackboard().get(blackboard::GBufferNormalMetallic);
        const auto gbuffer_world_position_roughness =
            context.blackboard().get(blackboard::GBufferWorldPositionRoughness);
        if (!isValidTextureHandle(scene_color) || !isValidTextureHandle(gbuffer_base_color) ||
            !isValidTextureHandle(gbuffer_normal_metallic) || !isValidTextureHandle(gbuffer_world_position_roughness)) {
            LUNA_RENDERER_WARN(
                "EditorInfiniteGrid missing input texture(s): color={} baseColor={} normal={} position={}",
                isValidTextureHandle(scene_color),
                isValidTextureHandle(gbuffer_base_color),
                isValidTextureHandle(gbuffer_normal_metallic),
                isValidTextureHandle(gbuffer_world_position_roughness));
            return;
        }

        const bool resources_ready = m_resources != nullptr && m_resources->ensure(scene_context);
        blackboard::publishSceneColorStage(
            context.blackboard(), blackboard::SceneColorStage::SkyComposited, *scene_color);

        context.graph().AddRasterPass(
            name(),
            [scene_color = *scene_color,
             gbuffer_base_color = *gbuffer_base_color,
             gbuffer_normal_metallic = *gbuffer_normal_metallic,
             gbuffer_world_position_roughness =
                 *gbuffer_world_position_roughness](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(gbuffer_base_color);
                pass_builder.ReadTexture(gbuffer_normal_metallic);
                pass_builder.ReadTexture(gbuffer_world_position_roughness);
                pass_builder.WriteColor(
                    scene_color, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
            },
            [this,
             options,
             resources_ready,
             gbuffer_base_color = *gbuffer_base_color,
             gbuffer_normal_metallic = *gbuffer_normal_metallic,
             gbuffer_world_position_roughness =
                 *gbuffer_world_position_roughness](RenderGraphRasterPassContext& pass_context) {
                if (!resources_ready || m_resources == nullptr || !m_resources->isComplete()) {
                    clearGridPass(pass_context);
                    return;
                }

                const auto& base_color_texture = pass_context.getTexture(gbuffer_base_color);
                const auto& normal_metallic_texture = pass_context.getTexture(gbuffer_normal_metallic);
                const auto& world_position_roughness_texture =
                    pass_context.getTexture(gbuffer_world_position_roughness);
                if (!base_color_texture || !normal_metallic_texture || !world_position_roughness_texture) {
                    clearGridPass(pass_context);
                    return;
                }

                m_resources->updateBindings(options,
                                            base_color_texture,
                                            normal_metallic_texture,
                                            world_position_roughness_texture,
                                            pass_context.framebufferWidth(),
                                            pass_context.framebufferHeight());
                m_resources->draw(pass_context);
            });
    }

private:
    [[nodiscard]] EditorInfiniteGridFeature::Options currentOptions() const
    {
        return m_options ? *m_options : EditorInfiniteGridFeature::Options{};
    }

    EditorInfiniteGridFeature::Resources* m_resources{nullptr};
    EditorInfiniteGridFeature::OptionsHandle m_options;
};

} // namespace

EditorInfiniteGridFeature::EditorInfiniteGridFeature()
    : EditorInfiniteGridFeature(std::make_shared<Options>())
{}

EditorInfiniteGridFeature::EditorInfiniteGridFeature(Options options)
    : EditorInfiniteGridFeature(std::make_shared<Options>(options))
{}

EditorInfiniteGridFeature::EditorInfiniteGridFeature(OptionsHandle options)
    : m_options(std::move(options)),
      m_resources(std::make_unique<Resources>())
{
    if (!m_options) {
        m_options = std::make_shared<Options>();
    }
}

EditorInfiniteGridFeature::~EditorInfiniteGridFeature() = default;

RenderFeatureContract EditorInfiniteGridFeature::contract() const noexcept
{
    return RenderFeatureContract{
        .name = kFeatureName,
        .display_name = "Editor Infinite Grid",
        .category = "Editor",
        .runtime_toggleable = true,
        .requirements =
            RenderFeatureRequirements{
                .scene_inputs = RenderFeatureSceneInputFlags::SceneColor |
                                RenderFeatureSceneInputFlags::GBufferBaseColor |
                                RenderFeatureSceneInputFlags::GBufferNormalMetallic |
                                RenderFeatureSceneInputFlags::GBufferWorldPositionRoughness,
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

bool EditorInfiniteGridFeature::enabled() const noexcept
{
    return m_options ? m_options->enabled : false;
}

std::vector<RenderFeatureParameterInfo> EditorInfiniteGridFeature::parameters() const
{
    const Options options = m_options ? *m_options : Options{};
    return {
        makeFloatParameter("gridScale", "Grid Scale", options.grid_scale, 0.01f, 100.0f, 0.01f),
        makeFloatParameter("fadeDistance", "Fade Distance", options.fade_distance, 10.0f, 5000.0f, 1.0f),
        makeFloatParameter("opacity", "Opacity", options.opacity, 0.0f, 1.0f, 0.01f),
    };
}

RenderFeatureDiagnostics EditorInfiniteGridFeature::diagnostics() const
{
    return m_resources ? m_resources->diagnostics() : RenderFeatureDiagnostics{};
}

bool EditorInfiniteGridFeature::setEnabled(bool enabled) noexcept
{
    if (!m_options) {
        return false;
    }

    m_options->enabled = enabled;
    return true;
}

bool EditorInfiniteGridFeature::setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept
{
    if (!m_options || value.type != RenderFeatureParameterType::Float) {
        return false;
    }

    if (name == "gridScale") {
        m_options->grid_scale = std::clamp(value.float_value, 0.01f, 100.0f);
        return true;
    }
    if (name == "fadeDistance") {
        m_options->fade_distance = std::clamp(value.float_value, 10.0f, 5000.0f);
        return true;
    }
    if (name == "opacity") {
        m_options->opacity = std::clamp(value.float_value, 0.0f, 1.0f);
        return true;
    }

    return false;
}

EditorInfiniteGridFeature::Options& EditorInfiniteGridFeature::options() noexcept
{
    return *m_options;
}

const EditorInfiniteGridFeature::Options& EditorInfiniteGridFeature::options() const noexcept
{
    return *m_options;
}

bool EditorInfiniteGridFeature::registerPasses(RenderFlowBuilder& builder)
{
    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered =
        builder.insertFeaturePassBetween(kFeatureName,
                                         extension_slots::AfterSky,
                                         extension_slots::BeforeTransparent,
                                         "EditorInfiniteGrid",
                                         std::make_unique<EditorInfiniteGridPass>(*m_resources, m_options));
    if (registered) {
        LUNA_RENDERER_INFO("Registered EditorInfiniteGrid between '{}' and '{}'",
                           extension_slots::AfterSky,
                           extension_slots::BeforeTransparent);
    }
    return registered;
}

void EditorInfiniteGridFeature::prepareFrame(const RenderWorld& world,
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

void EditorInfiniteGridFeature::shutdown()
{
    if (m_resources) {
        m_resources->shutdown();
    }
}

} // namespace luna::render_flow
