#include "Core/Log.h"
#include "Math/Math.h"
#include "Renderer/Material.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/Constants.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/ShadowPass.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/DefaultScene/ShadowCascadeBounds.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/Visibility/Frustum.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <glm/geometric.hpp>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 1> kShadowPassResources{{
    {.name = blackboard::ShadowMap.value(), .access = RenderPassResourceAccess::Write},
}};
constexpr float kCascadeShadowDistance = 120.0f;
constexpr float kCascadeSplitLambda = 0.55f;
constexpr float kCascadeLightDepthPadding = 4.0f;
constexpr float kCascadeOverlapScale = 0.12f;
constexpr float kMinCascadeOverlap = 0.5f;
constexpr float kMaxCascadeOverlap = 12.0f;
constexpr float kShadowDepthBias = 0.0018f;

uint32_t sanitizeShadowMapSize(uint32_t size, uint32_t fallback)
{
    constexpr uint32_t kMinShadowMapSize = 256;
    constexpr uint32_t kMaxShadowMapSize = 8'192;
    return std::clamp(size == 0 ? fallback : size, kMinShadowMapSize, kMaxShadowMapSize);
}

ShadowResources createShadowResources(RenderGraphBuilder& graph, uint32_t shadow_map_size, std::string_view name)
{
    return ShadowResources{
        .shadow_map = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = std::string(name),
            .Width = shadow_map_size,
            .Height = shadow_map_size,
            .Format = render_flow::default_scene_detail::kShadowMapFormat,
            .Usage = RHI::TextureUsageFlags::ColorAttachment | RHI::TextureUsageFlags::Sampled,
            .InitialState = RHI::ResourceState::Undefined,
            .SampleCount = RHI::SampleCount::Count1,
        }),
        .shadow_depth = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = std::string(name) + "Depth",
            .Width = shadow_map_size,
            .Height = shadow_map_size,
            .Format = RHI::Format::D32_FLOAT,
            .Usage = RHI::TextureUsageFlags::DepthStencilAttachment,
            .InitialState = RHI::ResourceState::Undefined,
            .SampleCount = RHI::SampleCount::Count1,
        }),
        .render_params = {},
    };
}

uint32_t csmCascadeSize(const RenderWorld* world)
{
    return sanitizeShadowMapSize(world != nullptr ? world->shadowSettings().csm_cascade_size : 2'048u, 2'048u);
}

uint32_t csmAtlasSize(uint32_t cascade_size)
{
    return cascade_size * render_flow::default_scene_detail::kShadowCascadeAtlasColumns;
}

uint32_t pcfShadowMapSize(const RenderWorld* world)
{
    return sanitizeShadowMapSize(world != nullptr ? world->shadowSettings().pcf_map_size : 4'096u, 4'096u);
}

ShadowResources createCascadedShadowResources(RenderGraphBuilder& graph, uint32_t cascade_size)
{
    return createShadowResources(graph, csmAtlasSize(cascade_size), "SceneShadowMap");
}

ShadowResources createPcfShadowResources(RenderGraphBuilder& graph, uint32_t shadow_map_size)
{
    return createShadowResources(graph, shadow_map_size, "ScenePcfShadowMap");
}

ShadowResources createDisabledShadowResources(RenderGraphBuilder& graph)
{
    return ShadowResources{
        .shadow_map = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneShadowMapDisabled",
            .Width = 1,
            .Height = 1,
            .Format = render_flow::default_scene_detail::kShadowMapFormat,
            .Usage = RHI::TextureUsageFlags::ColorAttachment | RHI::TextureUsageFlags::Sampled,
            .InitialState = RHI::ResourceState::Undefined,
            .SampleCount = RHI::SampleCount::Count1,
        }),
        .shadow_depth = {},
        .render_params = {},
    };
}

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length_squared = glm::dot(value, value);
    return length_squared > 1.0e-6f ? glm::normalize(value) : fallback;
}

void configureCascadeViewportAndScissor(RHI::CommandBufferEncoder& commands,
                                        uint32_t cascade_index,
                                        uint32_t cascade_size)
{
    const uint32_t atlas_columns = render_flow::default_scene_detail::kShadowCascadeAtlasColumns;
    const uint32_t tile_x = cascade_index % atlas_columns;
    const uint32_t tile_y = cascade_index / atlas_columns;
    const uint32_t offset_x = tile_x * cascade_size;
    const uint32_t offset_y = tile_y * cascade_size;

    commands.SetViewport({static_cast<float>(offset_x),
                          static_cast<float>(offset_y),
                          static_cast<float>(cascade_size),
                          static_cast<float>(cascade_size),
                          0.0f,
                          1.0f});
    commands.SetScissor({static_cast<int32_t>(offset_x), static_cast<int32_t>(offset_y), cascade_size, cascade_size});
}

void configurePcfViewportAndScissor(RHI::CommandBufferEncoder& commands, uint32_t shadow_map_size)
{
    commands.SetViewport(
        {0.0f, 0.0f, static_cast<float>(shadow_map_size), static_cast<float>(shadow_map_size), 0.0f, 1.0f});
    commands.SetScissor({0, 0, shadow_map_size, shadow_map_size});
}

float viewportAspectRatio(const SceneRenderContext& context)
{
    if (context.framebuffer_height == 0) {
        return 1.0f;
    }

    return std::max(static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height),
                    0.001f);
}

std::array<float, render_flow::default_scene_detail::kShadowCascadeCount> calculateCascadeSplits(float near_clip,
                                                                                                 float far_clip)
{
    std::array<float, render_flow::default_scene_detail::kShadowCascadeCount> splits{};
    const float clamped_near = std::max(near_clip, 0.001f);
    const float clamped_far = std::max(far_clip, clamped_near + 0.001f);

    for (uint32_t cascade_index = 0; cascade_index < render_flow::default_scene_detail::kShadowCascadeCount;
         ++cascade_index) {
        const float split_ratio = static_cast<float>(cascade_index + 1u) /
                                  static_cast<float>(render_flow::default_scene_detail::kShadowCascadeCount);
        const float linear_split = clamped_near + (clamped_far - clamped_near) * split_ratio;
        const float logarithmic_split = clamped_near * std::pow(clamped_far / clamped_near, split_ratio);
        splits[cascade_index] = kCascadeSplitLambda * logarithmic_split + (1.0f - kCascadeSplitLambda) * linear_split;
    }

    splits.back() = clamped_far;
    return splits;
}

float calculatePcfBiasScale(float near_clip, float far_clip)
{
    const auto csm_splits = calculateCascadeSplits(near_clip, far_clip);
    const float first_cascade_span = std::max(csm_splits[0], 0.001f);
    return std::sqrt(std::max(far_clip / first_cascade_span, 1.0f));
}

float calculateCascadeOverlap(float cascade_near, float cascade_far)
{
    const float cascade_span = std::max(cascade_far - cascade_near, 0.001f);
    return std::clamp(cascade_span * kCascadeOverlapScale, kMinCascadeOverlap, kMaxCascadeOverlap);
}

float pcfShadowDistance(const RenderWorld* world)
{
    if (world == nullptr) {
        return 40.0f;
    }

    return std::clamp(world->shadowSettings().pcf_shadow_distance, 1.0f, 1000.0f);
}

std::array<glm::vec3, 8>
    perspectiveFrustumCorners(const Camera& camera, float aspect_ratio, float near_distance, float far_distance)
{
    std::array<glm::vec3, 8> corners{};
    const auto& perspective = camera.getPerspectiveSettings();
    const glm::vec3 position = camera.getPosition();
    const glm::vec3 forward = camera.getForwardDirection();
    const glm::vec3 right = camera.getRightDirection();
    const glm::vec3 up = camera.getUpDirection();

    const float tan_half_fov = std::tan(perspective.vertical_fov_radians * 0.5f);
    const float near_half_height = tan_half_fov * near_distance;
    const float near_half_width = near_half_height * aspect_ratio;
    const float far_half_height = tan_half_fov * far_distance;
    const float far_half_width = far_half_height * aspect_ratio;

    const glm::vec3 near_center = position + forward * near_distance;
    const glm::vec3 far_center = position + forward * far_distance;

    corners[0] = near_center - right * near_half_width - up * near_half_height;
    corners[1] = near_center + right * near_half_width - up * near_half_height;
    corners[2] = near_center + right * near_half_width + up * near_half_height;
    corners[3] = near_center - right * near_half_width + up * near_half_height;
    corners[4] = far_center - right * far_half_width - up * far_half_height;
    corners[5] = far_center + right * far_half_width - up * far_half_height;
    corners[6] = far_center + right * far_half_width + up * far_half_height;
    corners[7] = far_center - right * far_half_width + up * far_half_height;
    return corners;
}

std::array<glm::vec3, 8> orthographicFrustumCorners(const Camera& camera, float aspect_ratio)
{
    std::array<glm::vec3, 8> corners{};
    const auto& orthographic = camera.getOrthographicSettings();
    const glm::vec3 position = camera.getPosition();
    const glm::vec3 forward = camera.getForwardDirection();
    const glm::vec3 right = camera.getRightDirection();
    const glm::vec3 up = camera.getUpDirection();
    const float half_height = orthographic.vertical_size * 0.5f;
    const float half_width = half_height * aspect_ratio;
    const float near_distance = orthographic.near_clip;
    const float far_distance = orthographic.far_clip;
    const glm::vec3 near_center = position + forward * near_distance;
    const glm::vec3 far_center = position + forward * far_distance;

    corners[0] = near_center - right * half_width - up * half_height;
    corners[1] = near_center + right * half_width - up * half_height;
    corners[2] = near_center + right * half_width + up * half_height;
    corners[3] = near_center - right * half_width + up * half_height;
    corners[4] = far_center - right * half_width - up * half_height;
    corners[5] = far_center + right * half_width - up * half_height;
    corners[6] = far_center + right * half_width + up * half_height;
    corners[7] = far_center - right * half_width + up * half_height;
    return corners;
}

void collectShadowCasterDrawCommandsForCascade(const std::vector<DrawCommand>& shadow_draw_commands,
                                               const Frustum& shadow_frustum,
                                               uint32_t cascade_index,
                                               std::vector<DrawCommand>& cascade_visible_draw_commands,
                                               std::vector<DrawCommand>& unique_visible_draw_commands,
                                               std::unordered_set<const DrawCommand*>& unique_visible_draw_command_ids,
                                               ShadowCullingStats& stats)
{
    cascade_visible_draw_commands.clear();
    cascade_visible_draw_commands.reserve(shadow_draw_commands.size());
    for (const DrawCommand& draw_command : shadow_draw_commands) {
        if (shadow_frustum.intersects(draw_command.world_bounds)) {
            cascade_visible_draw_commands.push_back(draw_command);
            ++stats.cascade_visible;
            ++stats.cascade_visible_by_index[cascade_index];
            if (unique_visible_draw_command_ids.insert(&draw_command).second) {
                unique_visible_draw_commands.push_back(draw_command);
            }
        } else {
            ++stats.cascade_culled;
            ++stats.cascade_culled_by_index[cascade_index];
        }
    }
}

struct CascadeShadowProjection {
    glm::mat4 view_projection{1.0f};
    float world_texel_size{0.0f};
};

CascadeShadowProjection buildCascadeShadowProjection(const std::array<glm::vec3, 8>& corners,
                                                     const glm::vec3& light_direction,
                                                     const RHI::RHIConventions& conventions,
                                                     uint32_t shadow_map_size,
                                                     const std::vector<DrawCommand>& shadow_draw_commands)
{
    render_flow::default_scene_detail::ShadowCascadeLightBounds bounds =
        render_flow::default_scene_detail::buildShadowCascadeReceiverBounds(corners, light_direction, shadow_map_size);
    const float world_texel_size =
        render_flow::default_scene_detail::shadowCascadeWorldTexelSize(bounds, shadow_map_size);
    for (const DrawCommand& draw_command : shadow_draw_commands) {
        (void) render_flow::default_scene_detail::expandShadowCascadeDepthForCaster(bounds, draw_command.world_bounds);
    }
    render_flow::default_scene_detail::padShadowCascadeDepth(bounds, kCascadeLightDepthPadding);
    return CascadeShadowProjection{
        .view_projection = render_flow::default_scene_detail::buildShadowCascadeViewProjection(bounds, conventions),
        .world_texel_size = world_texel_size,
    };
}

render_flow::default_scene_detail::ShadowRenderParams buildDirectionalShadowParams(const RenderWorld* world,
                                                                                   const SceneRenderContext& context,
                                                                                   const DrawQueue& draw_queue)
{
    render_flow::default_scene_detail::ShadowRenderParams params{};
    params.params = glm::vec4(0.0f,
                              kShadowDepthBias,
                              static_cast<float>(render_flow::default_scene_detail::kShadowCascadeCount),
                              1.0f / static_cast<float>(render_flow::default_scene_detail::kShadowCascadeTileSize));
    if (!world || world->shadowSettings().mode == RenderShadowMode::None || world->directionalLights().empty() ||
        draw_queue.drawCommands(luna::RenderPhase::ShadowCaster).empty()) {
        return params;
    }

    const RenderDirectionalLight& light = world->directionalLights().front();
    if (light.intensity <= 0.0f) {
        return params;
    }

    const glm::vec3 light_direction = safeNormalize(light.direction, glm::vec3(0.0f, 1.0f, 0.0f));
    const Camera& camera = world->camera();
    const float aspect_ratio = viewportAspectRatio(context);
    const std::vector<DrawCommand>& shadow_draw_commands = draw_queue.drawCommands(luna::RenderPhase::ShadowCaster);
    const RenderShadowMode shadow_mode = world->shadowSettings().mode;
    const uint32_t shadow_count =
        shadow_mode == RenderShadowMode::PcfShadowMap ? 1u : render_flow::default_scene_detail::kShadowCascadeCount;
    const uint32_t shadow_map_size =
        shadow_mode == RenderShadowMode::PcfShadowMap ? pcfShadowMapSize(world) : csmCascadeSize(world);
    params.params.z = static_cast<float>(shadow_count);
    params.params.w = 1.0f / static_cast<float>(shadow_map_size);
    if (shadow_mode == RenderShadowMode::PcfShadowMap) {
        params.atlas_params = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (camera.getProjectionType() == Camera::ProjectionType::Perspective) {
        const auto& perspective = camera.getPerspectiveSettings();
        const float near_clip = std::max(perspective.near_clip, 0.001f);
        const float far_clip = shadow_mode == RenderShadowMode::PcfShadowMap
                                   ? std::min(perspective.far_clip, pcfShadowDistance(world))
                                   : std::min(perspective.far_clip, kCascadeShadowDistance);
        if (shadow_mode == RenderShadowMode::PcfShadowMap) {
            const auto corners = perspectiveFrustumCorners(camera, aspect_ratio, near_clip, far_clip);
            const CascadeShadowProjection shadow_projection = buildCascadeShadowProjection(
                corners, light_direction, context.capabilities.conventions, shadow_map_size, shadow_draw_commands);
            params.view_projections[0] = shadow_projection.view_projection;
            params.cascade_texel_sizes[0] = shadow_projection.world_texel_size;
            params.cascade_splits[0] = far_clip;
            params.params.y = kShadowDepthBias * calculatePcfBiasScale(near_clip, far_clip);
            params.params.x = 1.0f;
            return params;
        }

        const auto splits = calculateCascadeSplits(near_clip, far_clip);

        float cascade_near = near_clip;
        for (uint32_t cascade_index = 0; cascade_index < shadow_count; ++cascade_index) {
            const float cascade_far = splits[cascade_index];
            const float overlap = calculateCascadeOverlap(cascade_near, cascade_far);
            const float shadow_near = cascade_index == 0u ? cascade_near : std::max(near_clip, cascade_near - overlap);
            const float shadow_far =
                cascade_index + 1u >= shadow_count ? cascade_far : std::min(far_clip, cascade_far + overlap);
            const auto corners = perspectiveFrustumCorners(camera, aspect_ratio, shadow_near, shadow_far);
            const CascadeShadowProjection shadow_projection = buildCascadeShadowProjection(
                corners, light_direction, context.capabilities.conventions, shadow_map_size, shadow_draw_commands);
            params.view_projections[cascade_index] = shadow_projection.view_projection;
            params.cascade_texel_sizes[cascade_index] = shadow_projection.world_texel_size;
            params.cascade_splits[cascade_index] = cascade_far;
            cascade_near = cascade_far;
        }
    } else {
        const auto corners = orthographicFrustumCorners(camera, aspect_ratio);
        const CascadeShadowProjection shadow_projection = buildCascadeShadowProjection(
            corners, light_direction, context.capabilities.conventions, shadow_map_size, shadow_draw_commands);
        for (uint32_t cascade_index = 0; cascade_index < shadow_count; ++cascade_index) {
            params.view_projections[cascade_index] = shadow_projection.view_projection;
            params.cascade_texel_sizes[cascade_index] = shadow_projection.world_texel_size;
            params.cascade_splits[cascade_index] = shadow_mode == RenderShadowMode::PcfShadowMap
                                                       ? std::max(camera.getOrthographicSettings().far_clip, 0.001f)
                                                       : static_cast<float>(cascade_index + 1u);
        }
        if (shadow_mode == RenderShadowMode::PcfShadowMap) {
            params.params.y = kShadowDepthBias * 3.0f;
        }
    }

    params.params.x = 1.0f;
    return params;
}

} // namespace

ShadowDepthPass::ShadowDepthPass(PassSharedState& state)
    : m_state(&state)
{}

const char* ShadowDepthPass::name() const noexcept
{
    return "SceneShadowDepth";
}

std::span<const RenderPassResourceUsage> ShadowDepthPass::resourceUsages() const noexcept
{
    return kShadowPassResources;
}

void ShadowDepthPass::setup(RenderPassContext& context)
{
    m_state->setShadowCullingStats({});

    const render_flow::default_scene_detail::ShadowRenderParams render_params =
        buildDirectionalShadowParams(m_state->world(), context.sceneContext(), m_state->drawQueue());
    const uint32_t shadow_count = static_cast<uint32_t>(render_params.params.z + 0.5f);
    const uint32_t cascade_size = csmCascadeSize(m_state->world());
    const uint32_t pcf_map_size = pcfShadowMapSize(m_state->world());
    ShadowResources shadow = render_params.params.x <= 0.5f
                                 ? createDisabledShadowResources(context.graph())
                                 : (shadow_count <= 1u ? createPcfShadowResources(context.graph(), pcf_map_size)
                                                       : createCascadedShadowResources(context.graph(), cascade_size));
    shadow.render_params = render_params;
    m_state->setShadowParams(shadow.render_params);

    context.blackboard().set(blackboard::ShadowMap, shadow.shadow_map);

    context.graph().AddRasterPass(
        name(),
        [shadow](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(shadow.shadow_map,
                                    RHI::AttachmentLoadOp::Clear,
                                    RHI::AttachmentStoreOp::Store,
                                    RHI::ClearValue::ColorFloat(1.0f, 1.0f, 1.0f, 1.0f));
            if (shadow.shadow_depth.isValid()) {
                pass_builder.WriteDepth(
                    shadow.shadow_depth, RHI::AttachmentLoadOp::Clear, RHI::AttachmentStoreOp::Store, {1.0f, 0});
            }
        },
        [this, scene_context = context.sceneContext()](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context);
        });
}

void ShadowDepthPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    if (m_state->shadowParams().params.x <= 0.5f) {
        pass_context.beginRendering();
        pass_context.endRendering();
        LUNA_RENDERER_FRAME_DEBUG("Scene shadow pass skipped because cascaded shadows are disabled or unavailable");
        return;
    }

    AssetCache& assets = m_state->assets();
    PipelineResources& pipelines = m_state->pipelines();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    const auto& shadow_draw_commands = draw_queue.drawCommands(luna::RenderPhase::ShadowCaster);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene shadow pass with {} shadow caster draw command(s)",
                              shadow_draw_commands.size());

    const DrawPassResources pass_resources = pipelines.shadowPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR("Scene shadow pass aborted: shadow_pipeline={} scene_descriptor_set={}",
                            static_cast<bool>(pass_resources.pipeline),
                            static_cast<bool>(pass_resources.scene_descriptor_set));
        return;
    }

    ShadowCullingStats shadow_culling_stats{};
    shadow_culling_stats.candidate_casters = static_cast<uint32_t>(
        std::min(shadow_draw_commands.size(), static_cast<size_t>((std::numeric_limits<uint32_t>::max)())));
    const uint32_t shadow_count = std::min(static_cast<uint32_t>(m_state->shadowParams().params.z + 0.5f),
                                           render_flow::default_scene_detail::kShadowCascadeCount);
    shadow_culling_stats.cascade_count = shadow_count;
    const uint32_t cascade_size = csmCascadeSize(m_state->world());
    const uint32_t pcf_map_size = pcfShadowMapSize(m_state->world());
    std::array<std::vector<DrawCommand>, render_flow::default_scene_detail::kShadowCascadeCount>
        cascade_shadow_draw_commands_by_index;
    std::vector<DrawCommand> unique_visible_shadow_draw_commands;
    unique_visible_shadow_draw_commands.reserve(shadow_draw_commands.size());
    std::unordered_set<const DrawCommand*> unique_visible_shadow_draw_command_ids;
    unique_visible_shadow_draw_command_ids.reserve(shadow_draw_commands.size());
    for (uint32_t cascade_index = 0; cascade_index < shadow_count; ++cascade_index) {
        const Frustum shadow_frustum =
            Frustum::fromViewProjection(m_state->shadowParams().view_projections[cascade_index]);
        collectShadowCasterDrawCommandsForCascade(shadow_draw_commands,
                                                  shadow_frustum,
                                                  cascade_index,
                                                  cascade_shadow_draw_commands_by_index[cascade_index],
                                                  unique_visible_shadow_draw_commands,
                                                  unique_visible_shadow_draw_command_ids,
                                                  shadow_culling_stats);
    }
    shadow_culling_stats.unique_visible = static_cast<uint32_t>(std::min(
        unique_visible_shadow_draw_commands.size(), static_cast<size_t>((std::numeric_limits<uint32_t>::max)())));
    shadow_culling_stats.unique_culled =
        shadow_culling_stats.candidate_casters > shadow_culling_stats.unique_visible
            ? shadow_culling_stats.candidate_casters - shadow_culling_stats.unique_visible
            : 0u;

    auto& commands = pass_context.commandBuffer();
    assets.prepareDraws(commands,
                        unique_visible_shadow_draw_commands,
                        default_material,
                        AssetCache::Bindings{
                            .device = pipelines.device(),
                            .descriptor_pool = pipelines.descriptorPool(),
                            .material_layout = pipelines.materialLayout(),
                        });

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    size_t recorded_draw_count = 0;
    for (uint32_t cascade_index = 0; cascade_index < shadow_count; ++cascade_index) {
        if (shadow_count <= 1u) {
            configurePcfViewportAndScissor(commands, pcf_map_size);
        } else {
            configureCascadeViewportAndScissor(commands, cascade_index, cascade_size);
        }
        recorded_draw_count += recordShadowDrawCommands(commands,
                                                        pass_resources,
                                                        cascade_shadow_draw_commands_by_index[cascade_index],
                                                        assets,
                                                        default_material,
                                                        cascade_index);
    }
    m_state->setShadowCullingStats(shadow_culling_stats);
    LUNA_RENDERER_FRAME_DEBUG("Scene shadow pass recorded {}/{} cascade-visible shadow candidate(s), unique visible "
                              "{}, unique culled {}, cascade-culled {} from {} caster command(s) across {} cascade(s)",
                              recorded_draw_count,
                              shadow_culling_stats.cascade_visible,
                              shadow_culling_stats.unique_visible,
                              shadow_culling_stats.unique_culled,
                              shadow_culling_stats.cascade_culled,
                              shadow_culling_stats.candidate_casters,
                              shadow_culling_stats.cascade_count);
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
