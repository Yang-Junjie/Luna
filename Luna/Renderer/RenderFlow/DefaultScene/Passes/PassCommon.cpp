#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/Environment.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderWorld/RenderWorld.h"

#include <array>
#include <optional>

namespace luna::render_flow::default_scene {

void configureViewportAndScissor(luna::RHI::CommandBufferEncoder& commands, uint32_t width, uint32_t height)
{
    commands.SetViewport({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f});
    commands.SetScissor({0, 0, width, height});
}

size_t recordDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                          const DrawPassResources& pass_resources,
                          const std::vector<DrawCommand>& draw_commands,
                          const AssetCache& assets,
                          const Material& default_material)
{
    size_t recorded_count = 0;
    for (const auto& draw_command : draw_commands) {
        const AssetCache::DrawResources draw_resources = assets.resolveDrawResources(draw_command, default_material);
        if (!draw_resources.isValid()) {
            LUNA_RENDERER_FRAME_TRACE(
                "Skipping draw command because GPU resources are incomplete: mesh='{}' submesh={} picking_id={}",
                draw_command.mesh ? draw_command.mesh->getName() : "<null>",
                draw_command.submesh_index,
                draw_command.picking_id);
            continue;
        }

        render_flow::default_scene_detail::MeshPushConstants push_constants{
            .model = draw_command.transform,
            .picking_id = draw_command.picking_id,
        };
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            draw_resources.material_descriptor_set,
            pass_resources.scene_descriptor_set,
        };

        commands.BindDescriptorSets(pass_resources.pipeline, 0, descriptor_sets);
        commands.PushConstants(pass_resources.pipeline,
                               luna::RHI::ShaderStage::Vertex,
                               0,
                               sizeof(render_flow::default_scene_detail::MeshPushConstants),
                               &push_constants);
        commands.BindVertexBuffer(0, draw_resources.vertex_buffer);
        commands.BindIndexBuffer(draw_resources.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.DrawIndexed(draw_resources.index_count, 1, 0, 0, 0);
        ++recorded_count;
    }
    return recorded_count;
}

size_t recordShadowDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                                const DrawPassResources& pass_resources,
                                const std::vector<DrawCommand>& draw_commands,
                                const AssetCache& assets,
                                const Material& default_material,
                                uint32_t cascade_index)
{
    size_t recorded_count = 0;
    for (const auto& draw_command : draw_commands) {
        const AssetCache::DrawResources draw_resources = assets.resolveDrawResources(draw_command, default_material);
        if (!draw_resources.hasGeometry()) {
            LUNA_RENDERER_FRAME_TRACE(
                "Skipping shadow draw command because geometry resources are incomplete: mesh='{}' submesh={}",
                draw_command.mesh ? draw_command.mesh->getName() : "<null>",
                draw_command.submesh_index);
            continue;
        }

        render_flow::default_scene_detail::MeshPushConstants push_constants{
            .model = draw_command.transform,
            .picking_id = draw_command.picking_id,
            .shadow_cascade_index = cascade_index,
        };
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{
            pass_resources.scene_descriptor_set,
        };

        commands.BindDescriptorSets(pass_resources.pipeline, 0, descriptor_sets);
        commands.PushConstants(pass_resources.pipeline,
                               luna::RHI::ShaderStage::Vertex,
                               0,
                               sizeof(render_flow::default_scene_detail::MeshPushConstants),
                               &push_constants);
        commands.BindVertexBuffer(0, draw_resources.vertex_buffer);
        commands.BindIndexBuffer(draw_resources.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.DrawIndexed(draw_resources.index_count, 1, 0, 0, 0);
        ++recorded_count;
    }
    return recorded_count;
}

void updateSceneParameters(PassSharedState& state, const SceneRenderContext& context)
{
    if (!state.world()) {
        LUNA_RENDERER_WARN("Scene parameter update skipped because no RenderWorld is active");
        return;
    }

    const RenderFeatureFrameContext* frame_context = state.frameContext();
    if (frame_context == nullptr) {
        LUNA_RENDERER_WARN("Scene parameter update skipped because no render feature frame context is active");
        return;
    }

    const EnvironmentResources& environment = state.environment();
    state.pipelines().updateSceneParameters(context,
                                            *state.world(),
                                            *frame_context,
                                            environment.prefilteredMaxMipLevel(),
                                            environment.irradianceSH(),
                                            state.shadowParams());
}

void updateEnvironmentBindings(PassSharedState& state)
{
    const EnvironmentResources& environment = state.environment();
    state.pipelines().updateSceneBindings(
        environment.sourceTexture().texture, environment.prefilteredTexture(), environment.brdfLutTexture());
}

RenderGraphTextureHandle readBlackboardTexture(const RenderPassBlackboard& blackboard,
                                               RenderResourceKey<RenderGraphTextureHandle> key,
                                               std::string_view pass_name)
{
    const std::optional<RenderGraphTextureHandle> handle = blackboard.get(key);
    if (handle.has_value() && handle->isValid()) {
        return *handle;
    }

    LUNA_RENDERER_WARN("Render pass '{}' could not find blackboard texture '{}'", pass_name, key.name);
    return {};
}

} // namespace luna::render_flow::default_scene
