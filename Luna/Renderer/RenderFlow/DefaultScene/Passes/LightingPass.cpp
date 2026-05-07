#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/LightingPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"

#include <array>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 12> kLightingPassResources{{
    {.name = blackboard::GBufferBaseColor.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::GBufferNormalMetallic.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::GBufferWorldPositionRoughness.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::GBufferEmissiveAo.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::Velocity.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::ShadowMap.value(), .access = RenderPassResourceAccess::Read},
    {.name = blackboard::Pick.value(), .access = RenderPassResourceAccess::Read},
    {.name = lighting_extension_keys::AmbientOcclusion,
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::Optional},
    {.name = lighting_extension_keys::Reflection,
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::Optional},
    {.name = lighting_extension_keys::IndirectDiffuse,
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::Optional},
    {.name = lighting_extension_keys::IndirectSpecular,
     .access = RenderPassResourceAccess::Read,
     .flags = RenderFeatureGraphResourceFlags::Optional},
    {.name = blackboard::SceneLitColor.value(),
     .access = RenderPassResourceAccess::Write,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

} // namespace

LightingPass::LightingPass(PassSharedState& state)
    : m_state(&state)
{}

const char* LightingPass::name() const noexcept
{
    return "SceneLighting";
}

std::span<const RenderPassResourceUsage> LightingPass::resourceUsages() const noexcept
{
    return kLightingPassResources;
}

void LightingPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    blackboard::publishSceneColorStage(
        context.blackboard(), blackboard::SceneColorStage::Lit, scene_context.color_target);
    const GBufferTextures gbuffer{
        .base_color = readBlackboardTexture(context.blackboard(), blackboard::GBufferBaseColor, name()),
        .normal_metallic = readBlackboardTexture(context.blackboard(), blackboard::GBufferNormalMetallic, name()),
        .world_position_roughness =
            readBlackboardTexture(context.blackboard(), blackboard::GBufferWorldPositionRoughness, name()),
        .emissive_ao = readBlackboardTexture(context.blackboard(), blackboard::GBufferEmissiveAo, name()),
        .velocity = readBlackboardTexture(context.blackboard(), blackboard::Velocity, name()),
    };
    const RenderGraphTextureHandle shadow_map =
        readBlackboardTexture(context.blackboard(), blackboard::ShadowMap, name());
    const RenderGraphTextureHandle pick_texture = readBlackboardTexture(context.blackboard(), blackboard::Pick, name());
    const LightingExtensionInputSet extension_inputs = getLightingExtensionInputs(context.blackboard());

    context.graph().AddRasterPass(
        name(),
        [gbuffer, shadow_map, pick_texture, extension_inputs, scene_context](
            RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(gbuffer.base_color);
            pass_builder.ReadTexture(gbuffer.normal_metallic);
            pass_builder.ReadTexture(gbuffer.world_position_roughness);
            pass_builder.ReadTexture(gbuffer.emissive_ao);
            pass_builder.ReadTexture(gbuffer.velocity);
            pass_builder.ReadTexture(shadow_map);
            pass_builder.ReadTexture(pick_texture);
            readLightingExtensionInputTextures(pass_builder, extension_inputs);
            pass_builder.WriteColor(scene_context.color_target,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(scene_context.clear_color.r,
                                                                      scene_context.clear_color.g,
                                                                      scene_context.clear_color.b,
                                                                      scene_context.clear_color.a));
        },
        [this, scene_context, gbuffer, shadow_map, pick_texture, extension_inputs](
            RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context, gbuffer, shadow_map, pick_texture, extension_inputs);
        });

    if (scene_context.debug_view_mode != RenderDebugViewMode::None && scene_context.debug_target.isValid()) {
        context.graph().AddRasterPass(
            "SceneDebugView",
            [gbuffer, pick_texture, scene_context](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(gbuffer.base_color);
                pass_builder.ReadTexture(gbuffer.normal_metallic);
                pass_builder.ReadTexture(gbuffer.world_position_roughness);
                pass_builder.ReadTexture(gbuffer.emissive_ao);
                pass_builder.ReadTexture(gbuffer.velocity);
                pass_builder.ReadTexture(pick_texture);
                pass_builder.WriteColor(scene_context.debug_target,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
            },
            [this, gbuffer, pick_texture](RenderGraphRasterPassContext& pass_context) {
                executeDebugView(pass_context, gbuffer, pick_texture);
            });
    }
}

void LightingPass::execute(RenderGraphRasterPassContext& pass_context,
                           const SceneRenderContext& context,
                           GBufferTextures gbuffer,
                           RenderGraphTextureHandle shadow_map_handle,
                           RenderGraphTextureHandle pick_texture_handle,
                           LightingExtensionInputSet extension_inputs)
{
    PipelineResources& pipelines = m_state->pipelines();
    LUNA_RENDERER_FRAME_DEBUG("Executing scene lighting pass");

    const LightingPassResources pass_resources = pipelines.lightingPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR("Scene lighting pass aborted: lighting_pipeline={} gbuffer_descriptor_set={} "
                            "lighting_scene_descriptor_set={} gbuffer_sampler={}",
                            static_cast<bool>(pass_resources.pipeline),
                            static_cast<bool>(pass_resources.gbuffer_descriptor_set),
                            static_cast<bool>(pass_resources.scene_descriptor_set),
                            static_cast<bool>(pass_resources.gbuffer_sampler));
        return;
    }

    const auto& gbuffer_base_color = pass_context.getTexture(gbuffer.base_color);
    const auto& gbuffer_normal_metallic = pass_context.getTexture(gbuffer.normal_metallic);
    const auto& gbuffer_world_position_roughness = pass_context.getTexture(gbuffer.world_position_roughness);
    const auto& gbuffer_emissive_ao = pass_context.getTexture(gbuffer.emissive_ao);
    const auto& velocity_texture = pass_context.getTexture(gbuffer.velocity);
    const auto& shadow_map = pass_context.getTexture(shadow_map_handle);
    const auto& pick_texture = pass_context.getTexture(pick_texture_handle);
    const LightingExtensionTextureRefs lighting_extensions =
        resolveLightingExtensionInputTextures(pass_context, extension_inputs);
    if (!gbuffer_base_color || !gbuffer_normal_metallic || !gbuffer_world_position_roughness || !gbuffer_emissive_ao ||
        !velocity_texture || !shadow_map || !pick_texture) {
        LUNA_RENDERER_WARN("Scene lighting pass aborted because one or more lighting textures are missing: base={} "
                           "normal={} position={} emissive={} velocity={} shadow={} pick={}",
                           static_cast<bool>(gbuffer_base_color),
                           static_cast<bool>(gbuffer_normal_metallic),
                           static_cast<bool>(gbuffer_world_position_roughness),
                           static_cast<bool>(gbuffer_emissive_ao),
                           static_cast<bool>(velocity_texture),
                           static_cast<bool>(shadow_map),
                           static_cast<bool>(pick_texture));
        return;
    }

    auto& commands = pass_context.commandBuffer();
    pipelines.updateLightingResources(commands,
                                      gbuffer_base_color,
                                      gbuffer_normal_metallic,
                                      gbuffer_world_position_roughness,
                                      gbuffer_emissive_ao,
                                      velocity_texture,
                                      pick_texture,
                                      lighting_extensions);
    pipelines.updateShadowResources(shadow_map);

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

void LightingPass::executeDebugView(RenderGraphRasterPassContext& pass_context,
                                    GBufferTextures gbuffer,
                                    RenderGraphTextureHandle pick_texture_handle)
{
    PipelineResources& pipelines = m_state->pipelines();
    LUNA_RENDERER_FRAME_DEBUG("Executing scene debug view pass");

    const DebugViewPassResources pass_resources = pipelines.debugViewPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR("Scene debug view pass aborted: debug_pipeline={} gbuffer_descriptor_set={} "
                            "scene_descriptor_set={} gbuffer_sampler={}",
                            static_cast<bool>(pass_resources.pipeline),
                            static_cast<bool>(pass_resources.gbuffer_descriptor_set),
                            static_cast<bool>(pass_resources.scene_descriptor_set),
                            static_cast<bool>(pass_resources.gbuffer_sampler));
        return;
    }

    const auto& gbuffer_base_color = pass_context.getTexture(gbuffer.base_color);
    const auto& gbuffer_normal_metallic = pass_context.getTexture(gbuffer.normal_metallic);
    const auto& gbuffer_world_position_roughness = pass_context.getTexture(gbuffer.world_position_roughness);
    const auto& gbuffer_emissive_ao = pass_context.getTexture(gbuffer.emissive_ao);
    const auto& velocity_texture = pass_context.getTexture(gbuffer.velocity);
    const auto& pick_texture = pass_context.getTexture(pick_texture_handle);
    if (!gbuffer_base_color || !gbuffer_normal_metallic || !gbuffer_world_position_roughness || !gbuffer_emissive_ao ||
        !velocity_texture || !pick_texture) {
        LUNA_RENDERER_WARN("Scene debug view pass aborted because one or more textures are missing: base={} normal={} "
                           "position={} emissive={} velocity={} pick={}",
                           static_cast<bool>(gbuffer_base_color),
                           static_cast<bool>(gbuffer_normal_metallic),
                           static_cast<bool>(gbuffer_world_position_roughness),
                           static_cast<bool>(gbuffer_emissive_ao),
                           static_cast<bool>(velocity_texture),
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
