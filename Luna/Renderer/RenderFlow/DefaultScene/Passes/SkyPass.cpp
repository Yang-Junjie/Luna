#include "Renderer/RenderFlow/DefaultScene/Passes/SkyPass.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RendererUtilities.h"

#include <array>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 5> kSkyPassResources{{
    {.name = blackboard::SceneLitColor.value(),
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::GBufferBaseColor.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::GBufferNormalMetallic.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::Pick.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::SceneSkyCompositedColor.value(),
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

} // namespace

SkyPass::SkyPass(PassSharedState& state) : m_state(&state) {}

const char* SkyPass::name() const noexcept
{
    return "SceneSky";
}

std::span<const RenderPassResourceUsage> SkyPass::resourceUsages() const noexcept
{
    return kSkyPassResources;
}

void SkyPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    blackboard::publishSceneColorStage(
        context.blackboard(), blackboard::SceneColorStage::SkyComposited, scene_context.color_target);
    const RenderGraphTextureHandle base_color =
        readBlackboardTexture(context.blackboard(), blackboard::GBufferBaseColor, name());
    const RenderGraphTextureHandle normal_metallic =
        readBlackboardTexture(context.blackboard(), blackboard::GBufferNormalMetallic, name());
    const RenderGraphTextureHandle pick_texture =
        readBlackboardTexture(context.blackboard(), blackboard::Pick, name());

    context.graph().AddRasterPass(
        name(),
        [base_color, normal_metallic, pick_texture, scene_context](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(base_color);
            pass_builder.ReadTexture(normal_metallic);
            pass_builder.ReadTexture(pick_texture);
            pass_builder.WriteColor(scene_context.color_target,
                                    luna::RHI::AttachmentLoadOp::Load,
                                    luna::RHI::AttachmentStoreOp::Store);
        },
        [this, base_color, normal_metallic, pick_texture](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, base_color, normal_metallic, pick_texture);
        });
}

void SkyPass::execute(RenderGraphRasterPassContext& pass_context,
                      RenderGraphTextureHandle base_color_handle,
                      RenderGraphTextureHandle normal_metallic_handle,
                      RenderGraphTextureHandle pick_texture_handle)
{
    PipelineResources& pipelines = m_state->pipelines();
    LUNA_RENDERER_FRAME_DEBUG("Executing scene sky pass");

    const SkyPassResources pass_resources = pipelines.skyPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR(
            "Scene sky pass aborted: sky_pipeline={} gbuffer_descriptor_set={} scene_descriptor_set={} gbuffer_sampler={}",
            static_cast<bool>(pass_resources.pipeline),
            static_cast<bool>(pass_resources.gbuffer_descriptor_set),
            static_cast<bool>(pass_resources.scene_descriptor_set),
            static_cast<bool>(pass_resources.gbuffer_sampler));
        return;
    }

    const auto& base_color = pass_context.getTexture(base_color_handle);
    const auto& normal_metallic = pass_context.getTexture(normal_metallic_handle);
    const auto& pick_texture = pass_context.getTexture(pick_texture_handle);
    if (!base_color || !normal_metallic || !pick_texture) {
        LUNA_RENDERER_WARN("Scene sky pass aborted because one or more textures are missing: base={} normal={} pick={}",
                           static_cast<bool>(base_color),
                           static_cast<bool>(normal_metallic),
                           static_cast<bool>(pick_texture));
        return;
    }

    auto& commands = pass_context.commandBuffer();
    pass_context.beginRendering();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
        pass_resources.gbuffer_descriptor_set,
        pass_resources.scene_descriptor_set,
    };
    commands.BindDescriptorSets(pass_resources.pipeline, 0, descriptor_sets);
    commands.Draw(3, 1, 0, 0);
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
