#include "Renderer/RenderFlow/DefaultScene/Passes/TransparentPass.h"

#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RendererUtilities.h"

#include <array>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 3> kTransparentPassResources{{
    {.name = blackboard::SceneColor.value(),
     .access = RenderPassResourceAccess::ReadWrite,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::Pick.value(), .access = RenderPassResourceAccess::ReadWrite},
    {.name = blackboard::Depth.value(),
     .access = RenderPassResourceAccess::ReadWrite,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

} // namespace

TransparentPass::TransparentPass(PassSharedState& state) : m_state(&state) {}

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
    if (m_state->drawQueue().drawCommands(luna::RenderPhase::Transparent).empty()) {
        return;
    }

    context.graph().AddRasterPass(
        name(),
        [scene_context](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(scene_context.color_target,
                                    luna::RHI::AttachmentLoadOp::Load,
                                    luna::RHI::AttachmentStoreOp::Store);
            pass_builder.WriteColor(scene_context.pick_target,
                                    luna::RHI::AttachmentLoadOp::Load,
                                    luna::RHI::AttachmentStoreOp::Store);
            pass_builder.WriteDepth(scene_context.depth_target,
                                    luna::RHI::AttachmentLoadOp::Load,
                                    luna::RHI::AttachmentStoreOp::Store);
        },
        [this, scene_context](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context);
        });
}

void TransparentPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    AssetCache& assets = m_state->assets();
    PipelineResources& pipelines = m_state->pipelines();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    auto transparent_draw_commands = draw_queue.drawCommands(luna::RenderPhase::Transparent);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene transparent pass with {} draw command(s)", transparent_draw_commands.size());

    const DrawPassResources pass_resources = pipelines.transparentPassResources();
    if (!pass_resources.isValid() || transparent_draw_commands.empty()) {
        LUNA_RENDERER_FRAME_DEBUG("Scene transparent pass skipped: transparent_pipeline={} scene_descriptor_set={} has_draws={}",
                                  static_cast<bool>(pass_resources.pipeline),
                                  static_cast<bool>(pass_resources.scene_descriptor_set),
                                  !transparent_draw_commands.empty());
        return;
    }

    draw_queue.sortBackToFront(transparent_draw_commands);

    auto& commands = pass_context.commandBuffer();
    assets.prepareDraws(commands, transparent_draw_commands, default_material, AssetCache::Bindings{
        .device = pipelines.device(),
        .descriptor_pool = pipelines.descriptorPool(),
        .material_layout = pipelines.materialLayout(),
    });

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const size_t recorded_draw_count =
        recordDrawCommands(commands, pass_resources, transparent_draw_commands, assets, default_material);
    LUNA_RENDERER_FRAME_DEBUG("Scene transparent pass recorded {}/{} draw command(s)",
                              recorded_draw_count,
                              transparent_draw_commands.size());
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
