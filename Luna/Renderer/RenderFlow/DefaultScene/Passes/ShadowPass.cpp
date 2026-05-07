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
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderWorld/RenderWorld.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <limits>
#include <string>
#include <string_view>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 1> kShadowPassResources{{
    {.name = blackboard::ShadowMap.value(), .access = RenderPassResourceAccess::Write},
}};
constexpr float kCascadeShadowDistance = 120.0f;
constexpr float kCascadeSplitLambda = 0.55f;
constexpr float kCascadeLightDepthPadding = 40.0f;
constexpr float kCascadeBoundsPaddingScale = 1.08f;
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
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .shadow_depth = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = std::string(name) + "Depth",
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
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .shadow_depth = {},
        .render_params = {},
    };
}

glm::mat4 adjustProjectionForConventions(glm::mat4 projection, const luna::RHI::RHIConventions& conventions)
{
    return conventions.requires_projection_y_flip ? luna::flipProjectionY(projection) : projection;
}

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length_squared = glm::dot(value, value);
    return length_squared > 1.0e-6f ? glm::normalize(value) : fallback;
}

void configureCascadeViewportAndScissor(luna::RHI::CommandBufferEncoder& commands,
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

void configurePcfViewportAndScissor(luna::RHI::CommandBufferEncoder& commands, uint32_t shadow_map_size)
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

    return (std::max)(static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height),
                      0.001f);
}

std::array<float, render_flow::default_scene_detail::kShadowCascadeCount> calculateCascadeSplits(float near_clip,
                                                                                                 float far_clip)
{
    std::array<float, render_flow::default_scene_detail::kShadowCascadeCount> splits{};
    const float clamped_near = (std::max)(near_clip, 0.001f);
    const float clamped_far = (std::max)(far_clip, clamped_near + 0.001f);

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
    const float first_cascade_span = (std::max)(csm_splits[0], 0.001f);
    return std::sqrt((std::max)(far_clip / first_cascade_span, 1.0f));
}

float calculateCascadeOverlap(float cascade_near, float cascade_far)
{
    const float cascade_span = (std::max)(cascade_far - cascade_near, 0.001f);
    return std::clamp(cascade_span * kCascadeOverlapScale, kMinCascadeOverlap, kMaxCascadeOverlap);
}

float pcfShadowDistance(const RenderWorld* world)
{
    if (world == nullptr) {
        return 40.0f;
    }

    return (std::clamp)(world->shadowSettings().pcf_shadow_distance, 1.0f, 1000.0f);
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

glm::mat4 buildCascadeViewProjection(const std::array<glm::vec3, 8>& corners,
                                     const glm::vec3& light_direction,
                                     const luna::RHI::RHIConventions& conventions,
                                     uint32_t shadow_map_size)
{
    glm::vec3 center{0.0f};
    for (const glm::vec3& corner : corners) {
        center += corner;
    }
    center /= static_cast<float>(corners.size());

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::length2(glm::cross(-light_direction, up)) <= 1.0e-6f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::mat4 view = glm::lookAtRH(center + light_direction, center, up);
    glm::vec3 min_bounds{std::numeric_limits<float>::max()};
    glm::vec3 max_bounds{std::numeric_limits<float>::lowest()};
    for (const glm::vec3& corner : corners) {
        const glm::vec3 light_space_corner = glm::vec3(view * glm::vec4(corner, 1.0f));
        min_bounds = glm::min(min_bounds, light_space_corner);
        max_bounds = glm::max(max_bounds, light_space_corner);
    }

    const glm::vec2 bounds_center = (glm::vec2(min_bounds) + glm::vec2(max_bounds)) * 0.5f;
    const glm::vec2 half_extent = (glm::vec2(max_bounds) - glm::vec2(min_bounds)) * (0.5f * kCascadeBoundsPaddingScale);
    min_bounds.x = bounds_center.x - half_extent.x;
    max_bounds.x = bounds_center.x + half_extent.x;
    min_bounds.y = bounds_center.y - half_extent.y;
    max_bounds.y = bounds_center.y + half_extent.y;

    const float cascade_width = max_bounds.x - min_bounds.x;
    const float cascade_height = max_bounds.y - min_bounds.y;
    const float texel_size_x = cascade_width / static_cast<float>((std::max)(shadow_map_size, 1u));
    const float texel_size_y = cascade_height / static_cast<float>((std::max)(shadow_map_size, 1u));
    if (texel_size_x > 0.0f && texel_size_y > 0.0f) {
        const glm::vec2 snapped_center{
            std::floor(bounds_center.x / texel_size_x) * texel_size_x,
            std::floor(bounds_center.y / texel_size_y) * texel_size_y,
        };
        min_bounds.x = snapped_center.x - half_extent.x;
        max_bounds.x = snapped_center.x + half_extent.x;
        min_bounds.y = snapped_center.y - half_extent.y;
        max_bounds.y = snapped_center.y + half_extent.y;
    }

    min_bounds.z -= kCascadeLightDepthPadding;
    max_bounds.z += kCascadeLightDepthPadding;

    const glm::mat4 projection =
        glm::orthoRH_ZO(min_bounds.x, max_bounds.x, min_bounds.y, max_bounds.y, -max_bounds.z, -min_bounds.z);
    return adjustProjectionForConventions(projection, conventions) * view;
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
        const float near_clip = (std::max)(perspective.near_clip, 0.001f);
        const float far_clip = shadow_mode == RenderShadowMode::PcfShadowMap
                                   ? (std::min)(perspective.far_clip, pcfShadowDistance(world))
                                   : (std::min)(perspective.far_clip, kCascadeShadowDistance);
        if (shadow_mode == RenderShadowMode::PcfShadowMap) {
            const auto corners = perspectiveFrustumCorners(camera, aspect_ratio, near_clip, far_clip);
            params.view_projections[0] =
                buildCascadeViewProjection(corners, light_direction, context.capabilities.conventions, shadow_map_size);
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
            const float shadow_near =
                cascade_index == 0u ? cascade_near : (std::max)(near_clip, cascade_near - overlap);
            const float shadow_far =
                cascade_index + 1u >= shadow_count ? cascade_far : (std::min)(far_clip, cascade_far + overlap);
            const auto corners = perspectiveFrustumCorners(camera, aspect_ratio, shadow_near, shadow_far);
            params.view_projections[cascade_index] =
                buildCascadeViewProjection(corners, light_direction, context.capabilities.conventions, shadow_map_size);
            params.cascade_splits[cascade_index] = cascade_far;
            cascade_near = cascade_far;
        }
    } else {
        const auto corners = orthographicFrustumCorners(camera, aspect_ratio);
        const glm::mat4 view_projection =
            buildCascadeViewProjection(corners, light_direction, context.capabilities.conventions, shadow_map_size);
        for (uint32_t cascade_index = 0; cascade_index < shadow_count; ++cascade_index) {
            params.view_projections[cascade_index] = view_projection;
            params.cascade_splits[cascade_index] = shadow_mode == RenderShadowMode::PcfShadowMap
                                                       ? (std::max)(camera.getOrthographicSettings().far_clip, 0.001f)
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
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(1.0f, 1.0f, 1.0f, 1.0f));
            if (shadow.shadow_depth.isValid()) {
                pass_builder.WriteDepth(shadow.shadow_depth,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        {1.0f, 0});
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
    const auto shadow_draw_commands = draw_queue.drawCommands(luna::RenderPhase::ShadowCaster);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene shadow pass with {} shadow caster draw command(s)",
                              shadow_draw_commands.size());

    const DrawPassResources pass_resources = pipelines.shadowPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR("Scene shadow pass aborted: shadow_pipeline={} scene_descriptor_set={}",
                            static_cast<bool>(pass_resources.pipeline),
                            static_cast<bool>(pass_resources.scene_descriptor_set));
        return;
    }

    auto& commands = pass_context.commandBuffer();
    assets.prepareDraws(commands,
                        shadow_draw_commands,
                        default_material,
                        AssetCache::Bindings{
                            .device = pipelines.device(),
                            .descriptor_pool = pipelines.descriptorPool(),
                            .material_layout = pipelines.materialLayout(),
                        });

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    size_t recorded_draw_count = 0;
    const uint32_t shadow_count = (std::min)(static_cast<uint32_t>(m_state->shadowParams().params.z + 0.5f),
                                             render_flow::default_scene_detail::kShadowCascadeCount);
    const uint32_t cascade_size = csmCascadeSize(m_state->world());
    const uint32_t pcf_map_size = pcfShadowMapSize(m_state->world());
    for (uint32_t cascade_index = 0; cascade_index < shadow_count; ++cascade_index) {
        if (shadow_count <= 1u) {
            configurePcfViewportAndScissor(commands, pcf_map_size);
        } else {
            configureCascadeViewportAndScissor(commands, cascade_index, cascade_size);
        }
        recorded_draw_count += recordShadowDrawCommands(
            commands, pass_resources, shadow_draw_commands, assets, default_material, cascade_index);
    }
    LUNA_RENDERER_FRAME_DEBUG("Scene shadow pass recorded {}/{} draw command(s) across {} cascade(s)",
                              recorded_draw_count,
                              shadow_draw_commands.size() * shadow_count,
                              shadow_count);
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
