#include "Renderer/RenderFlow/DefaultScene/Passes.h"

#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/PassNames.h"
#include "Renderer/RenderFlow/DefaultScene/ResourceManager.h"
#include "Renderer/RenderFlow/DefaultScene/Support.h"
#include "Renderer/RendererUtilities.h"

#include <algorithm>
#include <array>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <vector>

namespace luna::render_flow::default_scene {

namespace {

void configureViewportAndScissor(luna::RHI::CommandBufferEncoder& commands, uint32_t width, uint32_t height)
{
    commands.SetViewport({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f});
    commands.SetScissor({0, 0, width, height});
}

size_t recordDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                          const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& pipeline,
                          const std::vector<DrawCommand>& draw_commands,
                          const ResourceManager& resources,
                          const Material& default_material)
{
    size_t recorded_count = 0;
    for (const auto& draw_command : draw_commands) {
        const ResolvedDrawResources draw_resources = resources.resolveDrawResources(draw_command, default_material);
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
            resources.sceneDescriptorSet(),
        };

        commands.BindDescriptorSets(pipeline, 0, descriptor_sets);
        commands.PushConstants(
            pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(render_flow::default_scene_detail::MeshPushConstants), &push_constants);
        commands.BindVertexBuffer(0, draw_resources.vertex_buffer);
        commands.BindIndexBuffer(draw_resources.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.DrawIndexed(draw_resources.index_count, 1, 0, 0, 0);
        ++recorded_count;
    }
    return recorded_count;
}

size_t recordShadowDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                                const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& pipeline,
                                const std::vector<DrawCommand>& draw_commands,
                                const ResourceManager& resources,
                                const Material& default_material)
{
    size_t recorded_count = 0;
    for (const auto& draw_command : draw_commands) {
        const ResolvedDrawResources draw_resources = resources.resolveDrawResources(draw_command, default_material);
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
        };
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{
            resources.sceneDescriptorSet(),
        };

        commands.BindDescriptorSets(pipeline, 0, descriptor_sets);
        commands.PushConstants(
            pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(render_flow::default_scene_detail::MeshPushConstants), &push_constants);
        commands.BindVertexBuffer(0, draw_resources.vertex_buffer);
        commands.BindIndexBuffer(draw_resources.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.DrawIndexed(draw_resources.index_count, 1, 0, 0, 0);
        ++recorded_count;
    }
    return recorded_count;
}

DefaultSceneGBufferTextures createGBufferTextures(RenderGraphBuilder& graph, const SceneRenderContext& context)
{
    return DefaultSceneGBufferTextures{
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
    };
}

DefaultSceneShadowResources createShadowResources(RenderGraphBuilder& graph)
{
    const uint32_t shadow_map_size = render_flow::default_scene_detail::kShadowMapSize;
    return DefaultSceneShadowResources{
        .shadow_map = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneShadowMap",
            .Width = shadow_map_size,
            .Height = shadow_map_size,
            .Format = render_flow::default_scene_detail::kShadowMapFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .shadow_depth = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneShadowDepth",
            .Width = shadow_map_size,
            .Height = shadow_map_size,
            .Format = luna::RHI::Format::D32_FLOAT,
            .Usage = luna::RHI::TextureUsageFlags::DepthStencilAttachment,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .render_params = {},
    };
}

glm::mat4 adjustProjectionForBackend(glm::mat4 projection, luna::RHI::BackendType backend_type)
{
    if (backend_type == luna::RHI::BackendType::Vulkan) {
        projection[1][1] *= -1.0f;
    }
    return projection;
}

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length_squared = glm::dot(value, value);
    return length_squared > 1.0e-6f ? glm::normalize(value) : fallback;
}

render_flow::default_scene_detail::ShadowRenderParams
    buildDirectionalShadowParams(const RenderWorld* world, const SceneRenderContext& context, const DrawQueue& draw_queue)
{
    render_flow::default_scene_detail::ShadowRenderParams params{};
    params.params = glm::vec4(0.0f,
                              0.0018f,
                              0.0f,
                              1.0f / static_cast<float>(render_flow::default_scene_detail::kShadowMapSize));
    if (!world || world->directionalLights().empty() ||
        draw_queue.drawCommands(luna::RenderPhase::ShadowCaster).empty()) {
        return params;
    }

    const RenderDirectionalLight& light = world->directionalLights().front();
    if (light.intensity <= 0.0f) {
        return params;
    }

    constexpr float kShadowHalfExtent = 40.0f;
    constexpr float kShadowCameraDistance = 70.0f;
    constexpr float kShadowDepthRange = 160.0f;
    const glm::vec3 light_direction = safeNormalize(light.direction, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 camera_position = render_flow::default_scene_detail::resolveCameraPosition(world->camera());
    const glm::vec3 shadow_center = camera_position + world->camera().getForwardDirection() * 20.0f;
    const glm::vec3 shadow_eye = shadow_center + light_direction * kShadowCameraDistance;

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::length2(glm::cross(-light_direction, up)) <= 1.0e-6f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::mat4 view = glm::lookAtRH(shadow_eye, shadow_center, up);
    const glm::mat4 projection = glm::orthoRH_ZO(
        -kShadowHalfExtent, kShadowHalfExtent, -kShadowHalfExtent, kShadowHalfExtent, 0.0f, kShadowDepthRange);
    params.view_projection = adjustProjectionForBackend(projection, context.backend_type) * view;
    params.params.x = 1.0f;
    return params;
}

void updateSceneParameters(DefaultScenePassSharedState& state, const SceneRenderContext& context)
{
    if (!state.world()) {
        LUNA_RENDERER_WARN("Scene parameter update skipped because no RenderWorld is active");
        return;
    }

    state.resources().updateSceneParameters(context, *state.world(), state.shadow().render_params);
}

} // namespace

DefaultScenePassSharedState::DefaultScenePassSharedState(ResourceManager& resources, DrawQueue& draw_queue, Material& default_material)
    : m_resources(&resources), m_draw_queue(&draw_queue), m_default_material(&default_material)
{}

void DefaultScenePassSharedState::setWorld(const RenderWorld& world) noexcept
{
    m_world = &world;
}

ResourceManager& DefaultScenePassSharedState::resources() const noexcept
{
    return *m_resources;
}

DrawQueue& DefaultScenePassSharedState::drawQueue() const noexcept
{
    return *m_draw_queue;
}

Material& DefaultScenePassSharedState::defaultMaterial() const noexcept
{
    return *m_default_material;
}

const RenderWorld* DefaultScenePassSharedState::world() const noexcept
{
    return m_world;
}

DefaultSceneGBufferTextures& DefaultScenePassSharedState::gbuffer() noexcept
{
    return m_gbuffer;
}

const DefaultSceneGBufferTextures& DefaultScenePassSharedState::gbuffer() const noexcept
{
    return m_gbuffer;
}

DefaultSceneShadowResources& DefaultScenePassSharedState::shadow() noexcept
{
    return m_shadow;
}

const DefaultSceneShadowResources& DefaultScenePassSharedState::shadow() const noexcept
{
    return m_shadow;
}

DefaultSceneShadowDepthPass::DefaultSceneShadowDepthPass(DefaultScenePassSharedState& state) : m_state(&state) {}

const char* DefaultSceneShadowDepthPass::name() const noexcept
{
    return "SceneShadowDepth";
}

void DefaultSceneShadowDepthPass::setup(RenderPassContext& context)
{
    DefaultSceneShadowResources& shadow = m_state->shadow();
    shadow = createShadowResources(context.graph());
    shadow.render_params = buildDirectionalShadowParams(m_state->world(), context.sceneContext(), m_state->drawQueue());

    context.blackboard().setTexture(blackboard::ShadowMap, shadow.shadow_map);

    context.graph().AddRasterPass(
        name(),
        [shadow](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(shadow.shadow_map,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(1.0f, 1.0f, 1.0f, 1.0f));
            pass_builder.WriteDepth(shadow.shadow_depth,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    {1.0f, 0});
        },
        [this, scene_context = context.sceneContext()](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context);
        });
}

void DefaultSceneShadowDepthPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    ResourceManager& resources = m_state->resources();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    const auto shadow_draw_commands = draw_queue.drawCommands(luna::RenderPhase::ShadowCaster);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene shadow pass with {} shadow caster draw command(s)",
                              shadow_draw_commands.size());

    resources.ensurePipelines(context);
    if (!resources.shadowPipeline() || !resources.sceneDescriptorSet()) {
        LUNA_RENDERER_ERROR("Scene shadow pass aborted: shadow_pipeline={} scene_descriptor_set={}",
                            static_cast<bool>(resources.shadowPipeline()),
                            static_cast<bool>(resources.sceneDescriptorSet()));
        return;
    }

    auto& commands = pass_context.commandBuffer();
    updateSceneParameters(*m_state, context);
    resources.prepareDraws(commands, shadow_draw_commands, default_material);

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(resources.shadowPipeline());
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const size_t recorded_draw_count =
        recordShadowDrawCommands(commands, resources.shadowPipeline(), shadow_draw_commands, resources, default_material);
    LUNA_RENDERER_FRAME_DEBUG("Scene shadow pass recorded {}/{} draw command(s)",
                              recorded_draw_count,
                              shadow_draw_commands.size());
    pass_context.endRendering();
}

DefaultSceneGeometryPass::DefaultSceneGeometryPass(DefaultScenePassSharedState& state) : m_state(&state) {}

const char* DefaultSceneGeometryPass::name() const noexcept
{
    return "SceneGeometry";
}

void DefaultSceneGeometryPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    DefaultSceneGBufferTextures& gbuffer = m_state->gbuffer();
    gbuffer = createGBufferTextures(context.graph(), scene_context);

    context.blackboard().setTexture(blackboard::GBufferBaseColor, gbuffer.base_color);
    context.blackboard().setTexture(blackboard::GBufferNormalMetallic, gbuffer.normal_metallic);
    context.blackboard().setTexture(blackboard::GBufferWorldPositionRoughness, gbuffer.world_position_roughness);
    context.blackboard().setTexture(blackboard::GBufferEmissiveAo, gbuffer.emissive_ao);
    context.blackboard().setTexture(blackboard::SceneColor, scene_context.color_target);
    context.blackboard().setTexture(blackboard::Depth, scene_context.depth_target);
    context.blackboard().setTexture(blackboard::Pick, scene_context.pick_target);

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

void DefaultSceneGeometryPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    ResourceManager& resources = m_state->resources();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    const auto geometry_draw_commands = draw_queue.drawCommands(luna::RenderPhase::GBuffer);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene geometry pass with {} GBuffer draw command(s)", geometry_draw_commands.size());

    resources.ensurePipelines(context);
    if (!resources.geometryPipeline() || !resources.sceneDescriptorSet()) {
        LUNA_RENDERER_ERROR("Scene geometry pass aborted: graphics_pipeline={} scene_descriptor_set={}",
                            static_cast<bool>(resources.geometryPipeline()),
                            static_cast<bool>(resources.sceneDescriptorSet()));
        return;
    }

    auto& commands = pass_context.commandBuffer();
    updateSceneParameters(*m_state, context);
    resources.prepareDraws(commands, geometry_draw_commands, default_material);

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(resources.geometryPipeline());
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const size_t recorded_draw_count =
        recordDrawCommands(commands, resources.geometryPipeline(), geometry_draw_commands, resources, default_material);
    LUNA_RENDERER_FRAME_DEBUG("Scene geometry pass recorded {}/{} draw command(s)",
                              recorded_draw_count,
                              geometry_draw_commands.size());
    pass_context.endRendering();
}

DefaultSceneLightingPass::DefaultSceneLightingPass(DefaultScenePassSharedState& state) : m_state(&state) {}

const char* DefaultSceneLightingPass::name() const noexcept
{
    return "SceneLighting";
}

void DefaultSceneLightingPass::setup(RenderPassContext& context)
{
    const SceneRenderContext& scene_context = context.sceneContext();
    const DefaultSceneGBufferTextures gbuffer = m_state->gbuffer();
    const DefaultSceneShadowResources shadow = m_state->shadow();

    context.graph().AddRasterPass(
        name(),
        [gbuffer, shadow, scene_context](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(gbuffer.base_color);
            pass_builder.ReadTexture(gbuffer.normal_metallic);
            pass_builder.ReadTexture(gbuffer.world_position_roughness);
            pass_builder.ReadTexture(gbuffer.emissive_ao);
            pass_builder.ReadTexture(shadow.shadow_map);
            pass_builder.ReadTexture(scene_context.pick_target);
            pass_builder.WriteColor(scene_context.color_target,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(scene_context.clear_color.r,
                                                                     scene_context.clear_color.g,
                                                                     scene_context.clear_color.b,
                                                                     scene_context.clear_color.a));
        },
        [this, scene_context](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context);
        });
}

void DefaultSceneLightingPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    ResourceManager& resources = m_state->resources();
    const DefaultSceneGBufferTextures& gbuffer = m_state->gbuffer();
    LUNA_RENDERER_FRAME_DEBUG("Executing scene lighting pass");

    resources.ensurePipelines(context);
    if (!resources.lightingPipeline() || !resources.gbufferDescriptorSet() || !resources.sceneDescriptorSet() ||
        !resources.gbufferSampler()) {
        LUNA_RENDERER_ERROR(
            "Scene lighting pass aborted: lighting_pipeline={} gbuffer_descriptor_set={} scene_descriptor_set={} gbuffer_sampler={}",
            static_cast<bool>(resources.lightingPipeline()),
            static_cast<bool>(resources.gbufferDescriptorSet()),
            static_cast<bool>(resources.sceneDescriptorSet()),
            static_cast<bool>(resources.gbufferSampler()));
        return;
    }

    const auto& gbuffer_base_color = pass_context.getTexture(gbuffer.base_color);
    const auto& gbuffer_normal_metallic = pass_context.getTexture(gbuffer.normal_metallic);
    const auto& gbuffer_world_position_roughness = pass_context.getTexture(gbuffer.world_position_roughness);
    const auto& gbuffer_emissive_ao = pass_context.getTexture(gbuffer.emissive_ao);
    const auto& shadow_map = pass_context.getTexture(m_state->shadow().shadow_map);
    const auto& pick_texture = pass_context.getTexture(context.pick_target);
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
    resources.uploadEnvironmentIfNeeded(commands);
    updateSceneParameters(*m_state, context);
    resources.updateLightingResources(
        gbuffer_base_color, gbuffer_normal_metallic, gbuffer_world_position_roughness, gbuffer_emissive_ao, pick_texture);
    resources.updateShadowResources(shadow_map);

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(resources.lightingPipeline());
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
        resources.gbufferDescriptorSet(),
        resources.sceneDescriptorSet(),
    };
    commands.BindDescriptorSets(resources.lightingPipeline(), 0, descriptor_sets);
    commands.Draw(3, 1, 0, 0);
    pass_context.endRendering();
}

DefaultSceneTransparentPass::DefaultSceneTransparentPass(DefaultScenePassSharedState& state) : m_state(&state) {}

const char* DefaultSceneTransparentPass::name() const noexcept
{
    return "SceneTransparent";
}

void DefaultSceneTransparentPass::setup(RenderPassContext& context)
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

void DefaultSceneTransparentPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    ResourceManager& resources = m_state->resources();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    auto transparent_draw_commands = draw_queue.drawCommands(luna::RenderPhase::Transparent);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene transparent pass with {} draw command(s)", transparent_draw_commands.size());

    resources.ensurePipelines(context);
    if (!resources.transparentPipeline() || !resources.sceneDescriptorSet() || transparent_draw_commands.empty()) {
        LUNA_RENDERER_FRAME_DEBUG("Scene transparent pass skipped: transparent_pipeline={} scene_descriptor_set={} has_draws={}",
                                  static_cast<bool>(resources.transparentPipeline()),
                                  static_cast<bool>(resources.sceneDescriptorSet()),
                                  !transparent_draw_commands.empty());
        return;
    }

    draw_queue.sortBackToFront(transparent_draw_commands);

    auto& commands = pass_context.commandBuffer();
    updateSceneParameters(*m_state, context);
    resources.uploadEnvironmentIfNeeded(commands);
    resources.prepareDraws(commands, transparent_draw_commands, default_material);

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(resources.transparentPipeline());
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const size_t recorded_draw_count =
        recordDrawCommands(commands, resources.transparentPipeline(), transparent_draw_commands, resources, default_material);
    LUNA_RENDERER_FRAME_DEBUG("Scene transparent pass recorded {}/{} draw command(s)",
                              recorded_draw_count,
                              transparent_draw_commands.size());
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene



