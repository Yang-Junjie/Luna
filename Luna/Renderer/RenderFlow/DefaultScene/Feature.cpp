#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/Feature.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/EnvironmentPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/GBufferPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/LightingPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PostProcessPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/ShadowPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/SkyPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/TransparentPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/VisibilityBoundsOverlayPass.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlotPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderWorld/RenderWorld.h"

#include <algorithm>
#include <array>
#include <Backend.h>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace luna::render_flow::default_scene {
namespace {

inline constexpr std::string_view kFeatureName = "DefaultScene";
constexpr RenderFeatureGraphResourceFlags kOptionalExternalGraphResourceFlags =
    static_cast<RenderFeatureGraphResourceFlags>(static_cast<uint32_t>(RenderFeatureGraphResourceFlags::Optional) |
                                                 static_cast<uint32_t>(RenderFeatureGraphResourceFlags::External));

constexpr std::array<RenderFeatureGraphResource, 15> kGraphOutputs{{
    {.name = blackboard::SceneColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneLitColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneSkyCompositedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneTemporalResolvedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneTransparentCompositedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneBloomCompositedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneFinalColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::Depth.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {blackboard::Pick.value()},
    {blackboard::GBufferBaseColor.value()},
    {blackboard::GBufferNormalMetallic.value()},
    {blackboard::GBufferWorldPositionRoughness.value()},
    {blackboard::GBufferEmissiveAo.value()},
    {blackboard::Velocity.value()},
    {blackboard::ShadowMap.value()},
}};

constexpr std::array<RenderFeatureGraphResource, 15> kGraphInputs{{
    {blackboard::SceneLitColor.value()},
    {blackboard::SceneSkyCompositedColor.value()},
    {.name = blackboard::SceneTemporalResolvedColor.value(), .flags = kOptionalExternalGraphResourceFlags},
    {.name = blackboard::SceneBloomCompositedColor.value(), .flags = kOptionalExternalGraphResourceFlags},
    {blackboard::Pick.value()},
    {blackboard::GBufferBaseColor.value()},
    {blackboard::GBufferNormalMetallic.value()},
    {blackboard::GBufferWorldPositionRoughness.value()},
    {blackboard::GBufferEmissiveAo.value()},
    {blackboard::Velocity.value()},
    {blackboard::ShadowMap.value()},
    {.name = lighting_extension_keys::AmbientOcclusion, .flags = RenderFeatureGraphResourceFlags::Optional},
    {.name = lighting_extension_keys::Reflection, .flags = RenderFeatureGraphResourceFlags::Optional},
    {.name = lighting_extension_keys::IndirectDiffuse, .flags = RenderFeatureGraphResourceFlags::Optional},
    {.name = lighting_extension_keys::IndirectSpecular, .flags = RenderFeatureGraphResourceFlags::Optional},
}};

bool registerScenePasses(RenderFlowBuilder& builder,
                         PassSharedState& state,
                         VisibilityBoundsOverlayResources& visibility_bounds_overlay)
{
    namespace pass_slots = luna::render_flow::slots::passes;
    namespace extension_slots = luna::render_flow::slots::extension_points;

    return builder.addFeaturePass(
               kFeatureName, std::string(pass_slots::Environment), std::make_unique<EnvironmentPass>(state)) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          pass_slots::Environment,
                                          std::string(pass_slots::ShadowDepth),
                                          std::make_unique<ShadowDepthPass>(state)) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          pass_slots::ShadowDepth,
                                          std::string(pass_slots::GBuffer),
                                          std::make_unique<GeometryPass>(state)) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               pass_slots::GBuffer,
               std::string(extension_slots::AfterGBuffer),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterGBuffer))) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               extension_slots::AfterGBuffer,
               std::string(extension_slots::BeforeLighting),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforeLighting))) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          extension_slots::BeforeLighting,
                                          std::string(pass_slots::Lighting),
                                          std::make_unique<LightingPass>(state)) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               pass_slots::Lighting,
               std::string(extension_slots::AfterLighting),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterLighting))) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          extension_slots::AfterLighting,
                                          std::string(extension_slots::BeforeSky),
                                          std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforeSky))) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          extension_slots::BeforeSky,
                                          std::string(pass_slots::Sky),
                                          std::make_unique<SkyPass>(state)) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          pass_slots::Sky,
                                          std::string(extension_slots::AfterSky),
                                          std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterSky))) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               extension_slots::AfterSky,
               std::string(extension_slots::BeforeTransparent),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforeTransparent))) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          extension_slots::BeforeTransparent,
                                          std::string(pass_slots::Transparent),
                                          std::make_unique<TransparentPass>(state)) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               pass_slots::Transparent,
               std::string(extension_slots::AfterTransparent),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterTransparent))) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               extension_slots::AfterTransparent,
               std::string(extension_slots::BeforePostProcess),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforePostProcess))) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          extension_slots::BeforePostProcess,
                                          std::string(pass_slots::PostProcess),
                                          std::make_unique<PostProcessPass>(state)) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               pass_slots::PostProcess,
               std::string(extension_slots::AfterPostProcess),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterPostProcess))) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
               extension_slots::AfterPostProcess,
               std::string(extension_slots::BeforeOverlay),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforeOverlay))) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          extension_slots::BeforeOverlay,
                                          "VisibilityBoundsOverlay",
                                          std::make_unique<VisibilityBoundsOverlayPass>(
                                              state,
                                              visibility_bounds_overlay),
                                          100);
}

AssetCache::ClearMode toClearMode(PipelineResources::Invalidation invalidation)
{
    return invalidation == PipelineResources::Invalidation::All ? AssetCache::ClearMode::All
                                                                : AssetCache::ClearMode::MaterialsAndTextures;
}

bool drawStatsEqual(const DrawQueueStats& lhs, const DrawQueueStats& rhs)
{
    return lhs.submitted == rhs.submitted && lhs.camera_visible == rhs.camera_visible &&
           lhs.camera_culled == rhs.camera_culled && lhs.invalid_bounds == rhs.invalid_bounds &&
           lhs.shadow_unculled == rhs.shadow_unculled && lhs.phase_depth_only == rhs.phase_depth_only &&
           lhs.phase_gbuffer == rhs.phase_gbuffer && lhs.phase_forward_opaque == rhs.phase_forward_opaque &&
           lhs.phase_transparent == rhs.phase_transparent && lhs.phase_shadow_caster == rhs.phase_shadow_caster &&
           lhs.phase_picking == rhs.phase_picking;
}

RenderFeatureParameterInfo makeBoolParameter(std::string_view name,
                                             std::string_view display_name,
                                             bool value)
{
    RenderFeatureParameterInfo parameter{};
    parameter.name = name;
    parameter.display_name = display_name;
    parameter.type = RenderFeatureParameterType::Bool;
    parameter.value.type = RenderFeatureParameterType::Bool;
    parameter.value.bool_value = value;
    parameter.read_only = false;
    return parameter;
}

RenderFeatureRuntimeStat makeRuntimeUIntStat(std::string name, uint64_t value)
{
    return RenderFeatureRuntimeStat{
        .name = std::move(name),
        .type = RenderFeatureRuntimeStatType::UnsignedInteger,
        .uint_value = value,
    };
}

RenderFeatureRuntimeStat makeRuntimeFloatStat(std::string name, double value)
{
    return RenderFeatureRuntimeStat{
        .name = std::move(name),
        .type = RenderFeatureRuntimeStatType::Float,
        .float_value = value,
    };
}

RenderFeatureRuntimeStat makeRuntimeBoolStat(std::string name, bool value)
{
    return RenderFeatureRuntimeStat{
        .name = std::move(name),
        .type = RenderFeatureRuntimeStatType::Bool,
        .bool_value = value,
    };
}

double elapsedMilliseconds(std::chrono::steady_clock::time_point begin,
                           std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

Feature::Feature()
    : m_draw_queue(),
      m_environment(),
      m_assets(),
      m_pipelines(),
      m_default_material(),
      m_scene_state(m_assets, m_pipelines, m_draw_queue, m_environment, m_default_material)
{}

Feature::~Feature()
{
    shutdown();
}

RenderFeatureContract Feature::contract() const noexcept
{
    return RenderFeatureContract{
        .name = kFeatureName,
        .display_name = "Default Scene",
        .category = "Scene",
        .runtime_toggleable = false,
        .requirements =
            RenderFeatureRequirements{
                .resources = RenderFeatureResourceFlags::GraphicsPipeline | RenderFeatureResourceFlags::SampledTexture |
                             RenderFeatureResourceFlags::ColorAttachment | RenderFeatureResourceFlags::DepthAttachment |
                             RenderFeatureResourceFlags::UniformBuffer | RenderFeatureResourceFlags::Sampler,
                .rhi_capabilities = RenderFeatureRHICapabilityFlags::DefaultRenderFlow,
                .graph_inputs = kGraphInputs,
                .graph_outputs = kGraphOutputs,
                .requires_framebuffer_size = true,
                .uses_persistent_resources = true,
                .uses_history_resources = true,
            },
    };
}

bool Feature::registerPasses(RenderFlowBuilder& builder)
{
    return registerScenePasses(builder, m_scene_state, m_visibility_bounds_overlay);
}

std::vector<RenderFeatureParameterInfo> Feature::parameters() const
{
    return {
        makeBoolParameter("showVisibleBounds", "Show Visible Bounds", m_visibility_debug.show_visible_bounds),
        makeBoolParameter("showCulledBounds", "Show Culled Bounds", m_visibility_debug.show_culled_bounds),
        makeBoolParameter("showCullingFrustum", "Show Culling Frustum", m_visibility_debug.show_culling_frustum),
        makeBoolParameter("freezeCullingCamera", "Freeze Culling Camera", m_visibility_debug.freeze_culling_camera),
    };
}

bool Feature::setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept
{
    if (value.type != RenderFeatureParameterType::Bool) {
        return false;
    }

    if (name == "showVisibleBounds") {
        m_visibility_debug.show_visible_bounds = value.bool_value;
        return true;
    }
    if (name == "showCulledBounds") {
        m_visibility_debug.show_culled_bounds = value.bool_value;
        return true;
    }
    if (name == "showCullingFrustum") {
        m_visibility_debug.show_culling_frustum = value.bool_value;
        return true;
    }
    if (name == "freezeCullingCamera") {
        m_visibility_debug.freeze_culling_camera = value.bool_value;
        if (!m_visibility_debug.freeze_culling_camera) {
            m_has_frozen_culling_camera = false;
        }
        return true;
    }
    return false;
}

RenderFeatureDiagnostics Feature::diagnostics() const
{
    const VisibilityBoundsOverlayStats visibility_overlay_stats = m_visibility_bounds_overlay.stats();

    RenderFeatureDiagnostics diagnostics{};
    diagnostics.runtime_stats = {
        makeRuntimeUIntStat("draws.submitted", m_last_draw_stats.submitted),
        makeRuntimeFloatStat("culling.begin_scene_cpu_ms", m_last_draw_queue_begin_scene_cpu_ms),
        makeRuntimeFloatStat("culling.submit_packets_cpu_ms", m_last_draw_queue_submit_packets_cpu_ms),
        makeRuntimeFloatStat("culling.draw_queue_cpu_ms", m_last_draw_queue_cpu_ms),
        makeRuntimeFloatStat("culling.avg_us_per_submitted_draw", m_last_draw_queue_avg_us_per_submitted_draw),
        makeRuntimeUIntStat("draws.camera_visible", m_last_draw_stats.camera_visible),
        makeRuntimeUIntStat("draws.camera_culled", m_last_draw_stats.camera_culled),
        makeRuntimeUIntStat("draws.invalid_bounds", m_last_draw_stats.invalid_bounds),
        makeRuntimeUIntStat("draws.shadow_unculled", m_last_draw_stats.shadow_unculled),
        makeRuntimeUIntStat("draws.phase.depth_only", m_last_draw_stats.phase_depth_only),
        makeRuntimeUIntStat("draws.phase.gbuffer", m_last_draw_stats.phase_gbuffer),
        makeRuntimeUIntStat("draws.phase.forward_opaque", m_last_draw_stats.phase_forward_opaque),
        makeRuntimeUIntStat("draws.phase.transparent", m_last_draw_stats.phase_transparent),
        makeRuntimeUIntStat("draws.phase.shadow_caster", m_last_draw_stats.phase_shadow_caster),
        makeRuntimeUIntStat("draws.phase.picking", m_last_draw_stats.phase_picking),
        makeRuntimeBoolStat("visibility_debug.visible_enabled", m_visibility_debug.show_visible_bounds),
        makeRuntimeBoolStat("visibility_debug.culled_enabled", m_visibility_debug.show_culled_bounds),
        makeRuntimeBoolStat("visibility_debug.frustum_enabled", m_visibility_debug.show_culling_frustum),
        makeRuntimeBoolStat("visibility_debug.culling_camera_frozen", m_visibility_debug.freeze_culling_camera),
        makeRuntimeBoolStat("visibility_debug.has_frozen_culling_camera", m_has_frozen_culling_camera),
        makeRuntimeUIntStat("visibility_debug.captured", m_last_visibility_debug_stats.captured),
        makeRuntimeUIntStat("visibility_debug.camera_visible", m_last_visibility_debug_stats.camera_visible),
        makeRuntimeUIntStat("visibility_debug.camera_culled", m_last_visibility_debug_stats.camera_culled),
        makeRuntimeUIntStat("visibility_debug.invalid_bounds", m_last_visibility_debug_stats.invalid_bounds),
        makeRuntimeUIntStat("visibility_debug.culling_frustums", m_last_visibility_debug_stats.culling_frustums),
        makeRuntimeBoolStat("visibility_overlay.resources_ready", visibility_overlay_stats.resources_ready),
        makeRuntimeUIntStat("visibility_overlay.items", visibility_overlay_stats.build.items),
        makeRuntimeUIntStat("visibility_overlay.bounds", visibility_overlay_stats.build.bounds),
        makeRuntimeUIntStat("visibility_overlay.frustums", visibility_overlay_stats.build.frustums),
        makeRuntimeUIntStat("visibility_overlay.invalid_markers", visibility_overlay_stats.build.invalid_markers),
        makeRuntimeUIntStat("visibility_overlay.vertices", visibility_overlay_stats.build.vertices),
        makeRuntimeUIntStat("visibility_overlay.vertex_buffer_bytes", visibility_overlay_stats.vertex_buffer_bytes),
    };
    return diagnostics;
}

void Feature::prepareFrame(const RenderWorld& world,
                           const SceneRenderContext& scene_context,
                           const RenderFeatureFrameContext& frame_context,
                           RenderPassBlackboard& blackboard)
{
    namespace blackboard_names = luna::render_flow::blackboard;

    m_visibility_bounds_overlay.resetFrameStats();
    prepareResources(scene_context);

    blackboard_names::initializeSceneColorStageAliases(blackboard, scene_context.color_target);
    blackboard.set(blackboard_names::Depth, scene_context.depth_target);
    blackboard.set(blackboard_names::Pick, scene_context.pick_target);

    const float aspect_ratio = scene_context.framebuffer_height == 0
                                   ? 1.0f
                                   : (std::max) (static_cast<float>(scene_context.framebuffer_width) /
                                                     static_cast<float>(scene_context.framebuffer_height),
                                                 0.001f);
    if (m_visibility_debug.freeze_culling_camera && !m_has_frozen_culling_camera) {
        m_frozen_culling_camera = world.camera();
        m_frozen_culling_aspect_ratio = aspect_ratio;
        m_has_frozen_culling_camera = true;
        LUNA_RENDERER_DEBUG("Visibility debug culling camera frozen");
    }

    const Camera& culling_camera =
        m_visibility_debug.freeze_culling_camera && m_has_frozen_culling_camera ? m_frozen_culling_camera
                                                                               : world.camera();
    const float culling_aspect_ratio =
        m_visibility_debug.freeze_culling_camera && m_has_frozen_culling_camera ? m_frozen_culling_aspect_ratio
                                                                               : aspect_ratio;
    const auto draw_queue_begin = std::chrono::steady_clock::now();
    m_draw_queue.beginScene(world.camera(),
                            aspect_ratio,
                            culling_camera,
                            culling_aspect_ratio,
                            VisibilityDebugCaptureOptions{
                                .capture_visible_bounds = m_visibility_debug.show_visible_bounds,
                                .capture_culled_bounds = m_visibility_debug.show_culled_bounds,
                                .capture_culling_frustum = m_visibility_debug.show_culling_frustum,
                                .culling_frustum_frozen =
                                    m_visibility_debug.freeze_culling_camera && m_has_frozen_culling_camera,
                            });
    const auto submit_packets_begin = std::chrono::steady_clock::now();
    for (const auto& packet : world.drawPackets()) {
        m_draw_queue.submitDrawPacket(packet);
    }
    const auto draw_queue_end = std::chrono::steady_clock::now();
    m_last_draw_queue_begin_scene_cpu_ms = elapsedMilliseconds(draw_queue_begin, submit_packets_begin);
    m_last_draw_queue_submit_packets_cpu_ms = elapsedMilliseconds(submit_packets_begin, draw_queue_end);
    m_last_draw_queue_cpu_ms = elapsedMilliseconds(draw_queue_begin, draw_queue_end);
    m_last_draw_stats = m_draw_queue.stats();
    m_last_draw_queue_avg_us_per_submitted_draw =
        m_last_draw_stats.submitted > 0
            ? (m_last_draw_queue_cpu_ms * 1000.0) / static_cast<double>(m_last_draw_stats.submitted)
            : 0.0;
    m_last_visibility_debug_stats = m_draw_queue.visibilityDebugStats();
    LUNA_RENDERER_FRAME_DEBUG(
        "Scene draw culling: submitted={} camera_visible={} camera_culled={} invalid_bounds={} shadow_unculled={} culling_ms={:.3f} begin_ms={:.3f} submit_ms={:.3f} visibility_debug_items={} culling_frustums={}",
        m_last_draw_stats.submitted,
        m_last_draw_stats.camera_visible,
        m_last_draw_stats.camera_culled,
        m_last_draw_stats.invalid_bounds,
        m_last_draw_stats.shadow_unculled,
        m_last_draw_queue_cpu_ms,
        m_last_draw_queue_begin_scene_cpu_ms,
        m_last_draw_queue_submit_packets_cpu_ms,
        m_last_visibility_debug_stats.captured,
        m_last_visibility_debug_stats.culling_frustums);
    if (!m_has_logged_draw_stats || !drawStatsEqual(m_last_draw_stats, m_last_logged_draw_stats)) {
        LUNA_RENDERER_DEBUG("Scene draw culling stats: submitted={} camera_visible={} camera_culled={} "
                            "invalid_bounds={} shadow_unculled={} culling_ms={:.3f} begin_ms={:.3f} "
                            "submit_ms={:.3f} avg_us_per_draw={:.3f} "
                            "gbuffer={} transparent={} shadow={} picking={}",
                            m_last_draw_stats.submitted,
                            m_last_draw_stats.camera_visible,
                            m_last_draw_stats.camera_culled,
                            m_last_draw_stats.invalid_bounds,
                            m_last_draw_stats.shadow_unculled,
                            m_last_draw_queue_cpu_ms,
                            m_last_draw_queue_begin_scene_cpu_ms,
                            m_last_draw_queue_submit_packets_cpu_ms,
                            m_last_draw_queue_avg_us_per_submitted_draw,
                            m_last_draw_stats.phase_gbuffer,
                            m_last_draw_stats.phase_transparent,
                            m_last_draw_stats.phase_shadow_caster,
                            m_last_draw_stats.phase_picking);
        m_last_logged_draw_stats = m_last_draw_stats;
        m_has_logged_draw_stats = true;
    }
    m_scene_state.setWorld(world);
    m_scene_state.setFrameContext(frame_context);
    m_scene_state.setShadowParams({});
}

void Feature::prepareResources(const SceneRenderContext& scene_context)
{
    if (!scene_context.device || !scene_context.compiler) {
        LUNA_RENDERER_WARN("Cannot ensure scene render flow pipelines: device={} compiler={}",
                           static_cast<bool>(scene_context.device),
                           static_cast<bool>(scene_context.compiler));
        return;
    }

    const PipelineResources::Invalidation invalidation = m_pipelines.invalidationFor(scene_context);
    if (invalidation == PipelineResources::Invalidation::All) {
        LUNA_RENDERER_INFO("Scene render flow device changed; rebuilding GPU resources for backend '{}'",
                           luna::RHI::BackendTypeToString(scene_context.backend_type));
    } else if (invalidation == PipelineResources::Invalidation::MaterialsAndTextures) {
        LUNA_RENDERER_INFO("Rebuilding scene render flow pipeline state for backend '{}' and color format {} ({})",
                           luna::RHI::BackendTypeToString(scene_context.backend_type),
                           renderer_detail::formatToString(scene_context.color_format),
                           static_cast<int>(scene_context.color_format));
    }

    if (invalidation != PipelineResources::Invalidation::None) {
        m_assets.clear(toClearMode(invalidation));
        m_pipelines.shutdown();
        m_pipelines.rebuild(scene_context);
    }
}

void Feature::shutdown()
{
    if (m_shutdown) {
        return;
    }

    m_draw_queue.clear();
    m_visibility_bounds_overlay.shutdown();
    m_environment.reset();
    const bool had_pipeline_state = m_pipelines.hasAnyState();
    if (had_pipeline_state) {
        LUNA_RENDERER_INFO("Shutting down scene render flow resources");
    }
    m_assets.clear(AssetCache::ClearMode::All);
    m_pipelines.shutdown();
    if (had_pipeline_state) {
        LUNA_RENDERER_INFO("Scene render flow resources shutdown complete");
    }
    m_shutdown = true;
}

} // namespace luna::render_flow::default_scene
