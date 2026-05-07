#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/TransparentPass.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"

#include <array>
#include <DescriptorSet.h>
#include <Pipeline.h>
#include <Texture.h>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 6> kTransparentPassResources{{
    {.name = blackboard::SceneSkyCompositedColor.value(),
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneTemporalResolvedColor.value(),
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneTransparentCompositedColor.value(),
     .access = RenderPassResourceAccess::ReadWrite,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneFinalColor.value(),
     .access = RenderPassResourceAccess::ReadWrite,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::Pick.value(), .access = RenderPassResourceAccess::ReadWrite},
    {.name = blackboard::Depth.value(),
     .access = RenderPassResourceAccess::ReadWrite,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

RenderGraphTextureDesc makeTransparentColorDesc(const SceneRenderContext& scene_context)
{
    return RenderGraphTextureDesc{
        .Name = "SceneTransparentColor",
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

} // namespace

TransparentPass::TransparentPass(PassSharedState& state)
    : m_state(&state)
{}

const char* TransparentPass::name() const noexcept
{
    return "SceneTransparent";
}

std::span<const RenderPassResourceUsage> TransparentPass::resourceUsages() const noexcept
{
    return kTransparentPassResources;
}

void TransparentPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    blackboard::publishSceneColorStage(
        context.blackboard(), blackboard::SceneColorStage::TransparentComposited, scene_context.color_target);
    blackboard::publishSceneColorStage(
        context.blackboard(), blackboard::SceneColorStage::Final, scene_context.color_target);
    if (m_state->drawQueue().drawCommands(luna::RenderPhase::Transparent).empty()) {
        return;
    }

    const RenderGraphTextureHandle transparent_color =
        context.graph().CreateTexture(makeTransparentColorDesc(scene_context));
    if (!transparent_color.isValid()) {
        return;
    }

    context.graph().AddRasterPass(
        "SceneTransparentDraw",
        [scene_context, transparent_color](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(transparent_color,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(
                scene_context.pick_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
            pass_builder.WriteDepth(
                scene_context.depth_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
        },
        [this, scene_context](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context);
        });

    context.graph().AddRasterPass(
        "SceneTransparentComposite",
        [scene_context, transparent_color](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(transparent_color);
            pass_builder.WriteColor(
                scene_context.color_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
        },
        [this, transparent_color](RenderGraphRasterPassContext& pass_context) {
            composite(pass_context, transparent_color);
        });
}

void TransparentPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    AssetCache& assets = m_state->assets();
    PipelineResources& pipelines = m_state->pipelines();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    auto transparent_draw_commands = draw_queue.drawCommands(luna::RenderPhase::Transparent);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene transparent pass with {} draw command(s)",
                              transparent_draw_commands.size());

    const DrawPassResources pass_resources = pipelines.transparentPassResources();
    if (!pass_resources.isValid() || transparent_draw_commands.empty()) {
        LUNA_RENDERER_FRAME_DEBUG(
            "Scene transparent pass skipped: transparent_pipeline={} scene_descriptor_set={} has_draws={}",
            static_cast<bool>(pass_resources.pipeline),
            static_cast<bool>(pass_resources.scene_descriptor_set),
            !transparent_draw_commands.empty());
        return;
    }

    draw_queue.sortBackToFront(transparent_draw_commands);

    auto& commands = pass_context.commandBuffer();
    assets.prepareDraws(commands,
                        transparent_draw_commands,
                        default_material,
                        AssetCache::Bindings{
                            .device = pipelines.device(),
                            .descriptor_pool = pipelines.descriptorPool(),
                            .material_layout = pipelines.materialLayout(),
                        });

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const size_t recorded_draw_count =
        recordDrawCommands(commands, pass_resources, transparent_draw_commands, assets, default_material);
    LUNA_RENDERER_FRAME_DEBUG(
        "Scene transparent pass recorded {}/{} draw command(s)", recorded_draw_count, transparent_draw_commands.size());
    pass_context.endRendering();
}

void TransparentPass::composite(RenderGraphRasterPassContext& pass_context, RenderGraphTextureHandle transparent_color)
{
    PipelineResources& pipelines = m_state->pipelines();
    const TransparentCompositePassResources pass_resources = pipelines.transparentCompositePassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_FRAME_DEBUG("Scene transparent composite skipped: pipeline={} descriptor_set={} sampler={}",
                                  static_cast<bool>(pass_resources.pipeline),
                                  static_cast<bool>(pass_resources.descriptor_set),
                                  static_cast<bool>(pass_resources.sampler));
        return;
    }

    const auto& transparent_color_texture = pass_context.getTexture(transparent_color);
    if (!transparent_color_texture) {
        LUNA_RENDERER_FRAME_DEBUG("Scene transparent composite skipped because transparent color is missing");
        return;
    }

    pipelines.updateTransparentCompositeResources(transparent_color_texture);

    pass_context.beginRendering();
    auto& commands = pass_context.commandBuffer();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{
        pass_resources.descriptor_set,
    };
    commands.BindDescriptorSets(pass_resources.pipeline, 0, descriptor_sets);
    commands.Draw(3, 1, 0, 0);
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
