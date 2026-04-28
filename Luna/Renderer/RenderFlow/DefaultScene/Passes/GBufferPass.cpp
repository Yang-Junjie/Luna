#include "Renderer/RenderFlow/DefaultScene/Passes/GBufferPass.h"

#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/Constants.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RendererUtilities.h"

namespace luna::render_flow::default_scene {
namespace {

GBufferTextures createGBufferTextures(RenderGraphBuilder& graph, const SceneRenderContext& context)
{
    return GBufferTextures{
        .base_color = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferBaseColor",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = render_flow::default_scene_detail::kGBufferBaseColorFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .normal_metallic = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferNormalMetallic",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = render_flow::default_scene_detail::kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .world_position_roughness = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferWorldPositionRoughness",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = render_flow::default_scene_detail::kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .emissive_ao = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferEmissiveAo",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = render_flow::default_scene_detail::kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .velocity = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneVelocity",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = render_flow::default_scene_detail::kVelocityFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
    };
}

} // namespace

GeometryPass::GeometryPass(PassSharedState& state) : m_state(&state) {}

const char* GeometryPass::name() const noexcept
{
    return "SceneGeometry";
}

void GeometryPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    const GBufferTextures gbuffer = createGBufferTextures(context.graph(), scene_context);

    context.blackboard().set(blackboard::GBufferBaseColor, gbuffer.base_color);
    context.blackboard().set(blackboard::GBufferNormalMetallic, gbuffer.normal_metallic);
    context.blackboard().set(blackboard::GBufferWorldPositionRoughness, gbuffer.world_position_roughness);
    context.blackboard().set(blackboard::GBufferEmissiveAo, gbuffer.emissive_ao);
    context.blackboard().set(blackboard::Velocity, gbuffer.velocity);
    context.blackboard().set(blackboard::SceneColor, scene_context.color_target);
    context.blackboard().set(blackboard::Depth, scene_context.depth_target);
    context.blackboard().set(blackboard::Pick, scene_context.pick_target);

    context.graph().AddRasterPass(
        name(),
        [gbuffer, scene_context](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(gbuffer.base_color,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(gbuffer.normal_metallic,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(gbuffer.world_position_roughness,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(gbuffer.emissive_ao,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(gbuffer.velocity,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(scene_context.pick_target,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteDepth(scene_context.depth_target,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    {1.0f, 0});
        },
        [this, scene_context](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context);
        });
}

void GeometryPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    AssetCache& assets = m_state->assets();
    PipelineResources& pipelines = m_state->pipelines();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    const auto geometry_draw_commands = draw_queue.drawCommands(luna::RenderPhase::GBuffer);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene geometry pass with {} GBuffer draw command(s)", geometry_draw_commands.size());

    const DrawPassResources pass_resources = pipelines.geometryPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR("Scene geometry pass aborted: graphics_pipeline={} scene_descriptor_set={}",
                            static_cast<bool>(pass_resources.pipeline),
                            static_cast<bool>(pass_resources.scene_descriptor_set));
        return;
    }

    auto& commands = pass_context.commandBuffer();
    assets.prepareDraws(commands, geometry_draw_commands, default_material, AssetCache::Bindings{
        .device = pipelines.device(),
        .descriptor_pool = pipelines.descriptorPool(),
        .material_layout = pipelines.materialLayout(),
    });

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const size_t recorded_draw_count =
        recordDrawCommands(commands, pass_resources, geometry_draw_commands, assets, default_material);
    LUNA_RENDERER_FRAME_DEBUG("Scene geometry pass recorded {}/{} draw command(s)",
                              recorded_draw_count,
                              geometry_draw_commands.size());
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
