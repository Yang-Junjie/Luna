#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/Features/ScreenSpaceAmbientOcclusionFeature.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFeatureRegistry.h"
#include "Renderer/RenderFlow/RenderFeatureResources.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderFlow/ShaderBindingContract.h"
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

LUNA_REGISTER_RENDER_FEATURE(ScreenSpaceAmbientOcclusionFeature,
                             "ScreenSpaceAmbientOcclusion",
                             "Screen Space Ambient Occlusion",
                             "Lighting")

void linkScreenSpaceAmbientOcclusionFeature() {}

namespace {

inline constexpr std::string_view kFeatureName = "ScreenSpaceAmbientOcclusion";
constexpr luna::RHI::Format kAmbientOcclusionFormat = luna::RHI::Format::RGBA8_UNORM;

constexpr std::array<RenderFeatureGraphResource, 3> kGraphInputs{{
    {blackboard::Depth.value()},
    {blackboard::GBufferNormalMetallic.value()},
    {blackboard::GBufferWorldPositionRoughness.value()},
}};

constexpr std::array<RenderFeatureGraphResource, 1> kGraphOutputs{{
    {lighting_extension_keys::AmbientOcclusion},
}};

struct SsaoGpuParams {
    glm::vec4 framebuffer;
    glm::vec4 settings;
};

namespace ssao_binding {
constexpr uint32_t Depth = 0;
constexpr uint32_t NormalMetallic = 1;
constexpr uint32_t WorldPositionRoughness = 2;
constexpr uint32_t Sampler = 3;
constexpr uint32_t Params = 4;
} // namespace ssao_binding

struct SsaoDescriptorBindingSchema {
    const char* name{nullptr};
    const char* shader_name{nullptr};
    uint32_t binding{0};
    luna::RHI::DescriptorType type{luna::RHI::DescriptorType::UniformBuffer};
    uint32_t count{1};
    luna::RHI::ShaderStage stages{luna::RHI::ShaderStage::Fragment};
};

const std::array<SsaoDescriptorBindingSchema, 5> kSsaoBindings{{
    {"Depth",
     "gDepthTexture",
     ssao_binding::Depth,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"NormalMetallic",
     "gNormalMetallicTexture",
     ssao_binding::NormalMetallic,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"WorldPositionRoughness",
     "gWorldPositionRoughnessTexture",
     ssao_binding::WorldPositionRoughness,
     luna::RHI::DescriptorType::SampledImage,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"Sampler",
     "gSsaoSampler",
     ssao_binding::Sampler,
     luna::RHI::DescriptorType::Sampler,
     1,
     luna::RHI::ShaderStage::Fragment},
    {"Params",
     "gSsaoParams",
     ssao_binding::Params,
     luna::RHI::DescriptorType::UniformBuffer,
     1,
     luna::RHI::ShaderStage::Fragment},
}};

std::filesystem::path shaderPath()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders" /
           "ScreenSpaceAmbientOcclusion.slang";
}

luna::RHI::DescriptorSetLayoutCreateInfo makeSsaoDescriptorSetLayoutCreateInfo()
{
    luna::RHI::DescriptorSetLayoutCreateInfo create_info;
    create_info.Bindings.reserve(kSsaoBindings.size());
    for (const SsaoDescriptorBindingSchema& binding : kSsaoBindings) {
        create_info.Bindings.push_back(luna::RHI::DescriptorSetLayoutBinding{
            .Binding = binding.binding,
            .Type = binding.type,
            .Count = binding.count,
            .StageFlags = binding.stages,
        });
    }
    return create_info;
}

ShaderBindingContract makeSsaoShaderBindingContract()
{
    ShaderBindingContract contract = makeShaderBindingContract("ScreenSpaceAmbientOcclusion");
    contract.bindings.reserve(kSsaoBindings.size());
    for (const SsaoDescriptorBindingSchema& binding : kSsaoBindings) {
        contract.bindings.push_back(ShaderBindingRequirement{
            .name = binding.name ? binding.name : "",
            .shader_name = binding.shader_name ? binding.shader_name : "",
            .set_name = "ScreenSpaceAmbientOcclusion",
            .logical_set = 0,
            .logical_binding = binding.binding,
            .set = 0,
            .binding = binding.binding,
            .type = binding.type,
            .count = binding.count,
            .stages = binding.stages,
        });
    }
    return contract;
}

void validateSsaoShaderModuleBindings(const luna::RHI::Ref<luna::RHI::ShaderModule>& shader,
                                      const ShaderBindingContract& contract,
                                      const std::filesystem::path& shader_file,
                                      std::string_view entry_point)
{
    if (!shader) {
        return;
    }

    const std::string shader_file_string = shader_file.string();
    const ShaderBindingValidationResult result = validateShaderBindingContract(
        ShaderBindingValidationContext{
            .shader_file = shader_file_string,
            .entry_point = entry_point,
            .shader_stage = shader->GetStage(),
        },
        contract,
        shader->GetReflection());
    for (const std::string& issue : result.issues) {
        LUNA_RENDERER_WARN("Shader binding contract mismatch: {}", issue);
    }
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout>
    createDescriptorSetLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(makeSsaoDescriptorSetLayoutCreateInfo());
}

luna::RHI::Ref<luna::RHI::DescriptorPool> createDescriptorPool(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                            .SetMaxSets(8)
                                            .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 32)
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
                                     .SetFilter(luna::RHI::Filter::Nearest, luna::RHI::Filter::Nearest)
                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Nearest)
                                     .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToEdge)
                                     .SetAnisotropy(false)
                                     .SetName("ScreenSpaceAmbientOcclusionSampler")
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
                   const luna::RHI::Ref<luna::RHI::ShaderModule>& fragment_shader)
{
    if (!device || !layout || !vertex_shader || !fragment_shader) {
        return {};
    }

    return device->CreateGraphicsPipeline(luna::RHI::GraphicsPipelineBuilder()
                                              .SetShaders({vertex_shader, fragment_shader})
                                              .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
                                              .SetCullMode(luna::RHI::CullMode::None)
                                              .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
                                              .SetDepthTest(false, false, luna::RHI::CompareOp::Always)
                                              .AddColorAttachmentDefault(false)
                                              .AddColorFormat(kAmbientOcclusionFormat)
                                              .SetLayout(layout)
                                              .Build());
}

bool isValidTextureHandle(const std::optional<RenderGraphTextureHandle>& handle)
{
    return handle.has_value() && handle->isValid();
}

RenderGraphTextureDesc makeAmbientOcclusionGraphDesc(const SceneRenderContext& scene_context)
{
    return RenderGraphTextureDesc{
        .Name = "ScreenSpaceAmbientOcclusion",
        .Type = luna::RHI::TextureType::Texture2D,
        .Width = scene_context.framebuffer_width,
        .Height = scene_context.framebuffer_height,
        .Depth = 1,
        .ArrayLayers = 1,
        .MipLevels = 1,
        .Format = kAmbientOcclusionFormat,
        .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .InitialState = luna::RHI::ResourceState::Undefined,
        .SampleCount = luna::RHI::SampleCount::Count1,
    };
}

PersistentTexture2DDesc makeAmbientOcclusionPersistentDesc(const SceneRenderContext& scene_context)
{
    return PersistentTexture2DDesc{
        .width = scene_context.framebuffer_width,
        .height = scene_context.framebuffer_height,
        .format = kAmbientOcclusionFormat,
        .usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .initial_state = luna::RHI::ResourceState::Undefined,
        .sample_count = luna::RHI::SampleCount::Count1,
        .name = "ScreenSpaceAmbientOcclusion",
    };
}

void clearAmbientOcclusion(RenderGraphRasterPassContext& pass_context)
{
    pass_context.beginRendering();
    pass_context.endRendering();
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

} // namespace

class ScreenSpaceAmbientOcclusionFeature::Resources final {
public:
    void shutdown()
    {
        releasePersistentResources();
        m_state = {};
    }

    void releasePersistentResources()
    {
        m_ambient_occlusion.reset();
    }

    [[nodiscard]] bool ensure(const SceneRenderContext& context)
    {
        if (!context.device || !context.compiler) {
            return false;
        }

        if (m_state.device && m_state.device != context.device) {
            shutdown();
        }

        if (m_state.device == context.device && m_state.backend_type == context.backend_type && isComplete()) {
            return true;
        }

        shutdown();
        m_state.device = context.device;
        m_state.backend_type = context.backend_type;

        const std::filesystem::path path = shaderPath();
        m_state.vertex_shader = renderer_detail::loadShaderModule(
            m_state.device, context.compiler, path, "ssaoVertexMain", luna::RHI::ShaderStage::Vertex);
        m_state.fragment_shader = renderer_detail::loadShaderModule(
            m_state.device, context.compiler, path, "ssaoFragmentMain", luna::RHI::ShaderStage::Fragment);

        const ShaderBindingContract contract = makeSsaoShaderBindingContract();
        validateSsaoShaderModuleBindings(m_state.vertex_shader, contract, path, "ssaoVertexMain");
        validateSsaoShaderModuleBindings(m_state.fragment_shader, contract, path, "ssaoFragmentMain");

        m_state.layout = createDescriptorSetLayout(m_state.device);
        m_state.descriptor_pool = createDescriptorPool(m_state.device);
        m_state.pipeline_layout = createPipelineLayout(m_state.device, m_state.layout);
        m_state.pipeline =
            createPipeline(m_state.device, m_state.pipeline_layout, m_state.vertex_shader, m_state.fragment_shader);
        m_state.sampler = createSampler(m_state.device);
        m_state.params_buffer = m_state.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                 .SetSize(sizeof(SsaoGpuParams))
                                                                 .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                                 .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                 .SetName("ScreenSpaceAmbientOcclusionParams")
                                                                 .Build());

        if (m_state.descriptor_pool && m_state.layout) {
            m_state.descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.layout);
        }

        if (isComplete()) {
            LUNA_RENDERER_INFO("Created ScreenSpaceAmbientOcclusion pipeline for backend '{}'",
                               renderer_detail::backendTypeToString(context.backend_type));
            return true;
        }

        LUNA_RENDERER_WARN("ScreenSpaceAmbientOcclusion resources are incomplete: vs={} fs={} layout={} pool={} "
                           "pipeline_layout={} pipeline={} sampler={} params={} descriptor_set={}",
                           static_cast<bool>(m_state.vertex_shader),
                           static_cast<bool>(m_state.fragment_shader),
                           static_cast<bool>(m_state.layout),
                           static_cast<bool>(m_state.descriptor_pool),
                           static_cast<bool>(m_state.pipeline_layout),
                           static_cast<bool>(m_state.pipeline),
                           static_cast<bool>(m_state.sampler),
                           static_cast<bool>(m_state.params_buffer),
                           static_cast<bool>(m_state.descriptor_set));
        return false;
    }

    [[nodiscard]] bool ensureAmbientOcclusion(const SceneRenderContext& context)
    {
        return m_ambient_occlusion.ensure(context.device, makeAmbientOcclusionPersistentDesc(context));
    }

    [[nodiscard]] RenderGraphTextureHandle importAmbientOcclusion(RenderGraphBuilder& graph)
    {
        return importPersistentTexture2D(graph,
                                         m_ambient_occlusion,
                                         RenderFeatureTextureImportOptions{
                                             .name = "ScreenSpaceAmbientOcclusion",
                                             .final_state = luna::RHI::ResourceState::ShaderRead,
                                         });
    }

    [[nodiscard]] bool isComplete() const noexcept
    {
        return m_state.device && m_state.vertex_shader && m_state.fragment_shader && m_state.layout &&
               m_state.descriptor_pool && m_state.pipeline_layout && m_state.pipeline && m_state.sampler &&
               m_state.params_buffer && m_state.descriptor_set;
    }

    void updateBindings(const luna::RHI::Ref<luna::RHI::Texture>& depth,
                        const luna::RHI::Ref<luna::RHI::Texture>& normal_metallic,
                        const luna::RHI::Ref<luna::RHI::Texture>& world_position_roughness,
                        uint32_t width,
                        uint32_t height,
                        const Options& options)
    {
        if (!isComplete() || !depth || !normal_metallic || !world_position_roughness || width == 0 || height == 0) {
            return;
        }

        updateParams(width, height, options);
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = ssao_binding::Depth,
            .TextureView = depth->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = ssao_binding::NormalMetallic,
            .TextureView = normal_metallic->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = ssao_binding::WorldPositionRoughness,
            .TextureView = world_position_roughness->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
            .Binding = ssao_binding::Sampler,
            .Sampler = m_state.sampler,
        });
        m_state.descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
            .Binding = ssao_binding::Params,
            .Buffer = m_state.params_buffer,
            .Offset = 0,
            .Stride = sizeof(SsaoGpuParams),
            .Size = sizeof(SsaoGpuParams),
            .Type = luna::RHI::DescriptorType::UniformBuffer,
        });
        m_state.descriptor_set->Update();
    }

    void draw(RenderGraphRasterPassContext& pass_context) const
    {
        if (!isComplete()) {
            clearAmbientOcclusion(pass_context);
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

private:
    void updateParams(uint32_t width, uint32_t height, const Options& options)
    {
        const SsaoGpuParams params{
            .framebuffer = glm::vec4(1.0f / static_cast<float>(width),
                                     1.0f / static_cast<float>(height),
                                     static_cast<float>(width),
                                     static_cast<float>(height)),
            .settings = glm::vec4(std::max(options.radius, 0.01f),
                                  std::max(options.intensity, 0.0f),
                                  std::max(options.bias, 0.0f),
                                  std::max(options.power, 0.01f)),
        };
        if (void* mapped = m_state.params_buffer->Map()) {
            std::memcpy(mapped, &params, sizeof(params));
            m_state.params_buffer->Flush();
            m_state.params_buffer->Unmap();
        }
    }

    struct State {
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::BackendType backend_type{luna::RHI::BackendType::Auto};
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> layout;
        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;
        luna::RHI::Ref<luna::RHI::PipelineLayout> pipeline_layout;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> pipeline;
        luna::RHI::Ref<luna::RHI::Sampler> sampler;
        luna::RHI::Ref<luna::RHI::Buffer> params_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> descriptor_set;
        luna::RHI::Ref<luna::RHI::ShaderModule> vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> fragment_shader;
    };

    State m_state{};
    PersistentTexture2D m_ambient_occlusion;
};

namespace {

class ScreenSpaceAmbientOcclusionPass final : public IRenderPass {
public:
    ScreenSpaceAmbientOcclusionPass(ScreenSpaceAmbientOcclusionFeature::Resources& resources,
                                    ScreenSpaceAmbientOcclusionFeature::OptionsHandle options)
        : m_resources(&resources),
          m_options(std::move(options))
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "ScreenSpaceAmbientOcclusion";
    }

    void setup(RenderPassContext& context) override
    {
        const SceneRenderContext& scene_context = context.sceneContext();
        const ScreenSpaceAmbientOcclusionFeature::Options options = currentOptions();

        const auto depth = context.blackboard().get(blackboard::Depth);
        const auto normal_metallic = context.blackboard().get(blackboard::GBufferNormalMetallic);
        const auto world_position_roughness = context.blackboard().get(blackboard::GBufferWorldPositionRoughness);
        if (options.enabled && (!isValidTextureHandle(depth) || !isValidTextureHandle(normal_metallic) ||
                                !isValidTextureHandle(world_position_roughness))) {
            LUNA_RENDERER_WARN("ScreenSpaceAmbientOcclusion missing input texture(s): depth={} normal={} position={}",
                               isValidTextureHandle(depth),
                               isValidTextureHandle(normal_metallic),
                               isValidTextureHandle(world_position_roughness));
        }

        RenderGraphTextureHandle ambient_occlusion;
        if (m_resources != nullptr && m_resources->ensureAmbientOcclusion(scene_context)) {
            ambient_occlusion = m_resources->importAmbientOcclusion(context.graph());
        }
        if (!ambient_occlusion.isValid()) {
            ambient_occlusion = context.graph().CreateTexture(makeAmbientOcclusionGraphDesc(scene_context));
        }
        if (!ambient_occlusion.isValid()) {
            return;
        }

        setLightingExtensionInput(context.blackboard(), LightingExtensionInput::AmbientOcclusion, ambient_occlusion);

        const bool inputs_ready = isValidTextureHandle(depth) && isValidTextureHandle(normal_metallic) &&
                                  isValidTextureHandle(world_position_roughness);
        const bool resources_ready =
            options.enabled && inputs_ready && m_resources != nullptr && m_resources->ensure(scene_context);

        context.graph().AddRasterPass(
            name(),
            [ambient_occlusion, depth, normal_metallic, world_position_roughness, enabled = options.enabled](
                RenderGraphRasterPassBuilder& pass_builder) {
                if (enabled && isValidTextureHandle(depth)) {
                    pass_builder.ReadTexture(*depth);
                }
                if (enabled && isValidTextureHandle(normal_metallic)) {
                    pass_builder.ReadTexture(*normal_metallic);
                }
                if (enabled && isValidTextureHandle(world_position_roughness)) {
                    pass_builder.ReadTexture(*world_position_roughness);
                }
                pass_builder.WriteColor(ambient_occlusion,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(1.0f, 1.0f, 1.0f, 1.0f));
            },
            [this, options, resources_ready, depth, normal_metallic, world_position_roughness, scene_context](
                RenderGraphRasterPassContext& pass_context) {
                if (!options.enabled || !resources_ready || !isValidTextureHandle(depth) ||
                    !isValidTextureHandle(normal_metallic) || !isValidTextureHandle(world_position_roughness)) {
                    clearAmbientOcclusion(pass_context);
                    return;
                }

                const auto& depth_texture = pass_context.getTexture(*depth);
                const auto& normal_texture = pass_context.getTexture(*normal_metallic);
                const auto& position_texture = pass_context.getTexture(*world_position_roughness);
                if (!depth_texture || !normal_texture || !position_texture || m_resources == nullptr) {
                    clearAmbientOcclusion(pass_context);
                    return;
                }

                m_resources->updateBindings(depth_texture,
                                            normal_texture,
                                            position_texture,
                                            scene_context.framebuffer_width,
                                            scene_context.framebuffer_height,
                                            options);
                m_resources->draw(pass_context);
            });
    }

private:
    [[nodiscard]] ScreenSpaceAmbientOcclusionFeature::Options currentOptions() const
    {
        return m_options ? *m_options : ScreenSpaceAmbientOcclusionFeature::Options{};
    }

    ScreenSpaceAmbientOcclusionFeature::Resources* m_resources{nullptr};
    ScreenSpaceAmbientOcclusionFeature::OptionsHandle m_options;
};

} // namespace

ScreenSpaceAmbientOcclusionFeature::ScreenSpaceAmbientOcclusionFeature()
    : ScreenSpaceAmbientOcclusionFeature(std::make_shared<Options>())
{}

ScreenSpaceAmbientOcclusionFeature::ScreenSpaceAmbientOcclusionFeature(Options options)
    : ScreenSpaceAmbientOcclusionFeature(std::make_shared<Options>(options))
{}

ScreenSpaceAmbientOcclusionFeature::ScreenSpaceAmbientOcclusionFeature(OptionsHandle options)
    : m_options(std::move(options)),
      m_resources(std::make_unique<Resources>())
{
    if (!m_options) {
        m_options = std::make_shared<Options>();
    }
}

ScreenSpaceAmbientOcclusionFeature::~ScreenSpaceAmbientOcclusionFeature() = default;

RenderFeatureInfo ScreenSpaceAmbientOcclusionFeature::info() const noexcept
{
    return RenderFeatureInfo{
        .name = kFeatureName,
        .display_name = "Screen Space Ambient Occlusion",
        .category = "Lighting",
        .enabled = m_options ? m_options->enabled : true,
        .runtime_toggleable = true,
    };
}

std::vector<RenderFeatureParameterInfo> ScreenSpaceAmbientOcclusionFeature::parameters() const
{
    const Options options = m_options ? *m_options : Options{};
    return {
        makeFloatParameter("radius", "Radius", options.radius, 0.05f, 5.0f, 0.02f),
        makeFloatParameter("intensity", "Intensity", options.intensity, 0.0f, 4.0f, 0.02f),
        makeFloatParameter("bias", "Bias", options.bias, 0.0f, 0.3f, 0.001f),
        makeFloatParameter("power", "Power", options.power, 0.25f, 4.0f, 0.01f),
    };
}

RenderFeatureRequirements ScreenSpaceAmbientOcclusionFeature::requirements() const noexcept
{
    return RenderFeatureRequirements{
        .scene_inputs = RenderFeatureSceneInputFlags::Depth |
                        RenderFeatureSceneInputFlags::GBufferNormalMetallic |
                        RenderFeatureSceneInputFlags::GBufferWorldPositionRoughness,
        .resources = RenderFeatureResourceFlags::GraphicsPipeline | RenderFeatureResourceFlags::SampledTexture |
                     RenderFeatureResourceFlags::ColorAttachment | RenderFeatureResourceFlags::UniformBuffer |
                     RenderFeatureResourceFlags::Sampler,
        .lighting_outputs = RenderFeatureLightingOutputFlags::AmbientOcclusion,
        .rhi_capabilities = RenderFeatureRHICapabilityFlags::DefaultRenderFlow,
        .graph_inputs = kGraphInputs,
        .graph_outputs = kGraphOutputs,
        .requires_framebuffer_size = true,
        .uses_persistent_resources = true,
        .uses_history_resources = false,
    };
}

bool ScreenSpaceAmbientOcclusionFeature::setEnabled(bool enabled) noexcept
{
    if (!m_options) {
        return false;
    }

    m_options->enabled = enabled;
    return true;
}

bool ScreenSpaceAmbientOcclusionFeature::setParameter(std::string_view name,
                                                      const RenderFeatureParameterValue& value) noexcept
{
    if (!m_options || value.type != RenderFeatureParameterType::Float) {
        return false;
    }

    if (name == "radius") {
        m_options->radius = std::clamp(value.float_value, 0.05f, 5.0f);
        return true;
    }
    if (name == "intensity") {
        m_options->intensity = std::clamp(value.float_value, 0.0f, 4.0f);
        return true;
    }
    if (name == "bias") {
        m_options->bias = std::clamp(value.float_value, 0.0f, 0.3f);
        return true;
    }
    if (name == "power") {
        m_options->power = std::clamp(value.float_value, 0.25f, 4.0f);
        return true;
    }

    return false;
}

ScreenSpaceAmbientOcclusionFeature::Options& ScreenSpaceAmbientOcclusionFeature::options() noexcept
{
    return *m_options;
}

const ScreenSpaceAmbientOcclusionFeature::Options& ScreenSpaceAmbientOcclusionFeature::options() const noexcept
{
    return *m_options;
}

bool ScreenSpaceAmbientOcclusionFeature::registerPasses(RenderFlowBuilder& builder)
{
    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered =
        builder.insertFeaturePassBetween(kFeatureName,
                                         extension_slots::AfterGBuffer,
                                         extension_slots::BeforeLighting,
                                         "ScreenSpaceAmbientOcclusion",
                                         std::make_unique<ScreenSpaceAmbientOcclusionPass>(*m_resources, m_options));
    if (registered) {
        LUNA_RENDERER_INFO("Registered ScreenSpaceAmbientOcclusion between '{}' and '{}'",
                           extension_slots::AfterGBuffer,
                           extension_slots::BeforeLighting);
    }
    return registered;
}

void ScreenSpaceAmbientOcclusionFeature::prepareFrame(const RenderWorld& world,
                                                      const SceneRenderContext& scene_context,
                                                      const RenderFeatureFrameContext& frame_context,
                                                      RenderPassBlackboard& blackboard)
{
    (void) world;
    (void) scene_context;
    (void) frame_context;
    (void) blackboard;
}

void ScreenSpaceAmbientOcclusionFeature::releasePersistentResources()
{
    if (m_resources) {
        m_resources->releasePersistentResources();
    }
}

void ScreenSpaceAmbientOcclusionFeature::shutdown()
{
    if (m_resources) {
        m_resources->shutdown();
    }
}

} // namespace luna::render_flow
