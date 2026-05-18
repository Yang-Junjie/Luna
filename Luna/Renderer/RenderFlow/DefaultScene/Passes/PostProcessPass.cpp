#include "Renderer/RenderFlow/DefaultScene/Passes/PostProcessPass.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"

#include <array>
#include <DescriptorSet.h>
#include <Pipeline.h>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 2> kPostProcessPassResources{{
    {.name = blackboard::SceneBloomCompositedColor.value(),
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneFinalColor.value(),
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

} // namespace

PostProcessPass::PostProcessPass(PassSharedState& state) : m_state(&state) {}

const char* PostProcessPass::name() const noexcept
{
    return "ScenePostProcess";
}

std::span<const RenderPassResourceUsage> PostProcessPass::resourceUsages() const noexcept
{
    return kPostProcessPassResources;
}

void PostProcessPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    const RenderGraphTextureHandle scene_color =
        readBlackboardTexture(context.blackboard(), blackboard::SceneBloomCompositedColor, name());
    const auto requested_output = context.blackboard().get(blackboard::SceneFinalColor);
    const RenderGraphTextureHandle output =
        requested_output.has_value() && requested_output->isValid() ? *requested_output : scene_context.color_target;
    blackboard::publishSceneColorStage(context.blackboard(), blackboard::SceneColorStage::Final, output);
    if (!scene_color.isValid() || scene_color.Index == output.Index) {
        return;
    }

    context.graph().AddRasterPass(
        name(),
        [scene_color, output](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(scene_color);
            pass_builder.WriteColor(output,
                                    RHI::AttachmentLoadOp::Clear,
                                    RHI::AttachmentStoreOp::Store,
                                    RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
        },
        [this, scene_color](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_color);
        });
}

void PostProcessPass::execute(RenderGraphRasterPassContext& pass_context, RenderGraphTextureHandle scene_color)
{
    PipelineResources& pipelines = m_state->pipelines();
    const PostProcessPassResources pass_resources = pipelines.postProcessPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR("Scene post process pass aborted: pipeline={} descriptor_set={} sampler={}",
                            static_cast<bool>(pass_resources.pipeline),
                            static_cast<bool>(pass_resources.descriptor_set),
                            static_cast<bool>(pass_resources.sampler));
        return;
    }

    const auto& scene_color_texture = pass_context.getTexture(scene_color);
    if (!scene_color_texture) {
        LUNA_RENDERER_WARN("Scene post process pass aborted because scene color is missing");
        return;
    }

    pipelines.updatePostProcessResources(scene_color_texture);

    pass_context.beginRendering();
    auto& commands = pass_context.commandBuffer();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const std::array<RHI::Ref<RHI::DescriptorSet>, 1> descriptor_sets{
        pass_resources.descriptor_set,
    };
    commands.BindDescriptorSets(pass_resources.pipeline, 0, descriptor_sets);
    commands.Draw(3, 1, 0, 0);
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
