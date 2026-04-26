#include "Renderer/RenderFlow/DefaultScene/Passes/LightingPass.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RendererUtilities.h"

#include <array>
#include <string_view>

namespace luna::render_flow::default_scene {
namespace {

[[nodiscard]] RenderGraphTextureHandle optionalBlackboardTexture(const RenderPassBlackboard& blackboard,
                                                                 std::string_view name)
{
    return blackboard.getTexture(name).value_or(RenderGraphTextureHandle{});
}

void readOptionalTexture(RenderGraphRasterPassBuilder& pass_builder, RenderGraphTextureHandle handle)
{
    if (handle.isValid()) {
        pass_builder.ReadTexture(handle);
    }
}

[[nodiscard]] const luna::RHI::Ref<luna::RHI::Texture>&
    getOptionalTexture(RenderGraphRasterPassContext& pass_context, RenderGraphTextureHandle handle)
{
    return handle.isValid() ? pass_context.getTexture(handle) : pass_context.getTexture(RenderGraphTextureHandle{});
}

} // namespace

LightingPass::LightingPass(PassSharedState& state) : m_state(&state) {}

const char* LightingPass::name() const noexcept
{
    return "SceneLighting";
}

void LightingPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    const GBufferTextures gbuffer{
        .base_color = readBlackboardTexture(context.blackboard(), blackboard::GBufferBaseColor, name()),
        .normal_metallic = readBlackboardTexture(context.blackboard(), blackboard::GBufferNormalMetallic, name()),
        .world_position_roughness =
            readBlackboardTexture(context.blackboard(), blackboard::GBufferWorldPositionRoughness, name()),
        .emissive_ao = readBlackboardTexture(context.blackboard(), blackboard::GBufferEmissiveAo, name()),
    };
    const RenderGraphTextureHandle shadow_map =
        readBlackboardTexture(context.blackboard(), blackboard::ShadowMap, name());
    const RenderGraphTextureHandle pick_texture =
        readBlackboardTexture(context.blackboard(), blackboard::Pick, name());
    const ExtensionInputTextures extension_inputs{
        .ambient_occlusion = optionalBlackboardTexture(context.blackboard(), blackboard::AmbientOcclusion),
        .reflection = optionalBlackboardTexture(context.blackboard(), blackboard::Reflection),
        .indirect_diffuse = optionalBlackboardTexture(context.blackboard(), blackboard::IndirectDiffuse),
        .indirect_specular = optionalBlackboardTexture(context.blackboard(), blackboard::IndirectSpecular),
    };

    context.graph().AddRasterPass(
        name(),
        [gbuffer, shadow_map, pick_texture, extension_inputs, scene_context](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(gbuffer.base_color);
            pass_builder.ReadTexture(gbuffer.normal_metallic);
            pass_builder.ReadTexture(gbuffer.world_position_roughness);
            pass_builder.ReadTexture(gbuffer.emissive_ao);
            pass_builder.ReadTexture(shadow_map);
            pass_builder.ReadTexture(pick_texture);
            readOptionalTexture(pass_builder, extension_inputs.ambient_occlusion);
            readOptionalTexture(pass_builder, extension_inputs.reflection);
            readOptionalTexture(pass_builder, extension_inputs.indirect_diffuse);
            readOptionalTexture(pass_builder, extension_inputs.indirect_specular);
            pass_builder.WriteColor(scene_context.color_target,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(scene_context.clear_color.r,
                                                                     scene_context.clear_color.g,
                                                                     scene_context.clear_color.b,
                                                                     scene_context.clear_color.a));
        },
        [this, scene_context, gbuffer, shadow_map, pick_texture, extension_inputs](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context, gbuffer, shadow_map, pick_texture, extension_inputs);
        });
}

void LightingPass::execute(RenderGraphRasterPassContext& pass_context,
                           const SceneRenderContext& context,
                           GBufferTextures gbuffer,
                           RenderGraphTextureHandle shadow_map_handle,
                           RenderGraphTextureHandle pick_texture_handle,
                           ExtensionInputTextures extension_inputs)
{
    PipelineResources& pipelines = m_state->pipelines();
    LUNA_RENDERER_FRAME_DEBUG("Executing scene lighting pass");

    const LightingPassResources pass_resources = pipelines.lightingPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR(
            "Scene lighting pass aborted: lighting_pipeline={} gbuffer_descriptor_set={} lighting_scene_descriptor_set={} gbuffer_sampler={}",
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
    const auto& shadow_map = pass_context.getTexture(shadow_map_handle);
    const auto& pick_texture = pass_context.getTexture(pick_texture_handle);
    const auto& ambient_occlusion = getOptionalTexture(pass_context, extension_inputs.ambient_occlusion);
    const auto& reflection = getOptionalTexture(pass_context, extension_inputs.reflection);
    const auto& indirect_diffuse = getOptionalTexture(pass_context, extension_inputs.indirect_diffuse);
    const auto& indirect_specular = getOptionalTexture(pass_context, extension_inputs.indirect_specular);
    if (!gbuffer_base_color || !gbuffer_normal_metallic || !gbuffer_world_position_roughness || !gbuffer_emissive_ao ||
        !shadow_map || !pick_texture) {
        LUNA_RENDERER_WARN("Scene lighting pass aborted because one or more lighting textures are missing: base={} normal={} position={} emissive={} shadow={} pick={}",
                           static_cast<bool>(gbuffer_base_color),
                           static_cast<bool>(gbuffer_normal_metallic),
                           static_cast<bool>(gbuffer_world_position_roughness),
                           static_cast<bool>(gbuffer_emissive_ao),
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
                                      pick_texture,
                                      ambient_occlusion,
                                      reflection,
                                      indirect_diffuse,
                                      indirect_specular);
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

} // namespace luna::render_flow::default_scene
