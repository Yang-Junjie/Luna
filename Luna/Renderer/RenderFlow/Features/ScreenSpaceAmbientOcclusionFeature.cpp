#include "Renderer/RenderFlow/Features/ScreenSpaceAmbientOcclusionFeature.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/Resources/ShaderModuleLoader.h"

#include <Builders.h>
#include <Buffer.h>
#include <CommandBufferEncoder.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <Texture.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <glm/vec4.hpp>
#include <memory>
#include <string>

namespace luna::render_flow {
namespace {

constexpr luna::RHI::Format kAmbientOcclusionFormat = luna::RHI::Format::RGBA8_UNORM;

struct SsaoGpuParams {
    glm::vec4 framebuffer;
    glm::vec4 settings;
};

std::filesystem::path shaderPath()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders" /
           "ScreenSpaceAmbientOcclusion.slang";
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout>
    createDescriptorSetLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(3, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(4, luna::RHI::DescriptorType::UniformBuffer, 1, luna::RHI::ShaderStage::Fragment)
            .Build());
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

void clearAmbientOcclusion(RenderGraphRasterPassContext& pass_context)
{
    pass_context.beginRendering();
    pass_context.endRendering();
}

} // namespace

class ScreenSpaceAmbientOcclusionFeature::Resources final {
public:
    void shutdown()
    {
        m_state = {};
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

        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = 0,
            .TextureView = depth->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = 1,
            .TextureView = normal_metallic->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = 2,
            .TextureView = world_position_roughness->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        m_state.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
            .Binding = 3,
            .Sampler = m_state.sampler,
        });
        m_state.descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
            .Binding = 4,
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
};

namespace {

class ScreenSpaceAmbientOcclusionPass final : public IRenderPass {
public:
    ScreenSpaceAmbientOcclusionPass(ScreenSpaceAmbientOcclusionFeature::Resources& resources,
                                    ScreenSpaceAmbientOcclusionFeature::Options options)
        : m_resources(&resources),
          m_options(options)
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "ScreenSpaceAmbientOcclusion";
    }

    void setup(RenderPassContext& context) override
    {
        const SceneRenderContext& scene_context = context.sceneContext();

        const auto depth = context.blackboard().getTexture(blackboard::Depth);
        const auto normal_metallic = context.blackboard().getTexture(blackboard::GBufferNormalMetallic);
        const auto world_position_roughness = context.blackboard().getTexture(blackboard::GBufferWorldPositionRoughness);
        if (!isValidTextureHandle(depth) || !isValidTextureHandle(normal_metallic) ||
            !isValidTextureHandle(world_position_roughness)) {
            LUNA_RENDERER_WARN("ScreenSpaceAmbientOcclusion missing input texture(s): depth={} normal={} position={}",
                               isValidTextureHandle(depth),
                               isValidTextureHandle(normal_metallic),
                               isValidTextureHandle(world_position_roughness));
        }

        const bool resources_ready = m_resources != nullptr && m_resources->ensure(scene_context);

        RenderGraphTextureHandle ambient_occlusion = context.graph().CreateTexture(RenderGraphTextureDesc{
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
        });
        if (!ambient_occlusion.isValid()) {
            return;
        }

        context.blackboard().setTexture(blackboard::AmbientOcclusion, ambient_occlusion);
        context.graph().AddRasterPass(
            name(),
            [ambient_occlusion, depth, normal_metallic, world_position_roughness](RenderGraphRasterPassBuilder& pass_builder) {
                if (isValidTextureHandle(depth)) {
                    pass_builder.ReadTexture(*depth);
                }
                if (isValidTextureHandle(normal_metallic)) {
                    pass_builder.ReadTexture(*normal_metallic);
                }
                if (isValidTextureHandle(world_position_roughness)) {
                    pass_builder.ReadTexture(*world_position_roughness);
                }
                pass_builder.WriteColor(ambient_occlusion,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(1.0f, 1.0f, 1.0f, 1.0f));
            },
            [this, resources_ready, depth, normal_metallic, world_position_roughness, scene_context](
                RenderGraphRasterPassContext& pass_context) {
                if (!resources_ready || !isValidTextureHandle(depth) || !isValidTextureHandle(normal_metallic) ||
                    !isValidTextureHandle(world_position_roughness)) {
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
                                            m_options);
                m_resources->draw(pass_context);
            });
    }

private:
    ScreenSpaceAmbientOcclusionFeature::Resources* m_resources{nullptr};
    ScreenSpaceAmbientOcclusionFeature::Options m_options{};
};

} // namespace

ScreenSpaceAmbientOcclusionFeature::ScreenSpaceAmbientOcclusionFeature()
    : m_resources(std::make_unique<Resources>())
{}

ScreenSpaceAmbientOcclusionFeature::ScreenSpaceAmbientOcclusionFeature(Options options)
    : m_options(options),
      m_resources(std::make_unique<Resources>())
{}

ScreenSpaceAmbientOcclusionFeature::~ScreenSpaceAmbientOcclusionFeature() = default;

bool ScreenSpaceAmbientOcclusionFeature::registerPasses(RenderFlowBuilder& builder)
{
    if (!m_options.enabled) {
        LUNA_RENDERER_INFO("ScreenSpaceAmbientOcclusionFeature disabled; no pass registered");
        return true;
    }

    namespace extension_slots = luna::render_flow::slots::extension_points;

    const bool registered =
        builder.insertPassBetween(extension_slots::AfterGBuffer,
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
                                                      RenderPassBlackboard& blackboard)
{
    (void) world;
    (void) scene_context;
    (void) blackboard;
}

void ScreenSpaceAmbientOcclusionFeature::shutdown()
{
    if (m_resources) {
        m_resources->shutdown();
    }
}

} // namespace luna::render_flow
