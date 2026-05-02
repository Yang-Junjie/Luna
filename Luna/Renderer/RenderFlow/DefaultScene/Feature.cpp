#include "Renderer/RenderFlow/DefaultScene/Feature.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/EnvironmentPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/GBufferPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/LightingPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/ShadowPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/SkyPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/TransparentPass.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlotPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"

#include <Backend.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace luna::render_flow::default_scene {
namespace {

inline constexpr std::string_view kFeatureName = "DefaultScene";
constexpr RenderFeatureGraphResourceFlags kOptionalExternalGraphResourceFlags =
    static_cast<RenderFeatureGraphResourceFlags>(
        static_cast<uint32_t>(RenderFeatureGraphResourceFlags::Optional) |
        static_cast<uint32_t>(RenderFeatureGraphResourceFlags::External));

constexpr std::array<RenderFeatureGraphResource, 14> kGraphOutputs{{
    {.name = blackboard::SceneColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneLitColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneSkyCompositedColor.value(), .flags = RenderFeatureGraphResourceFlags::External},
    {.name = blackboard::SceneTransparentCompositedColor.value(),
     .flags = RenderFeatureGraphResourceFlags::External},
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

constexpr std::array<RenderFeatureGraphResource, 14> kGraphInputs{{
    {blackboard::SceneLitColor.value()},
    {blackboard::SceneSkyCompositedColor.value()},
    {.name = blackboard::SceneTemporalResolvedColor.value(), .flags = kOptionalExternalGraphResourceFlags},
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

bool registerScenePasses(RenderFlowBuilder& builder, PassSharedState& state)
{
    namespace pass_slots = luna::render_flow::slots::passes;
    namespace extension_slots = luna::render_flow::slots::extension_points;

    return builder.addFeaturePass(kFeatureName,
                                  std::string(pass_slots::Environment),
                                  std::make_unique<EnvironmentPass>(state)) &&
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
           builder.insertFeaturePassAfter(
               kFeatureName,
               extension_slots::AfterLighting,
               std::string(extension_slots::BeforeSky),
               std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforeSky))) &&
           builder.insertFeaturePassAfter(kFeatureName,
                                          extension_slots::BeforeSky,
                                          std::string(pass_slots::Sky),
                                          std::make_unique<SkyPass>(state)) &&
           builder.insertFeaturePassAfter(
               kFeatureName,
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
               std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterTransparent)));
}

AssetCache::ClearMode toClearMode(PipelineResources::Invalidation invalidation)
{
    return invalidation == PipelineResources::Invalidation::All ? AssetCache::ClearMode::All
                                                                : AssetCache::ClearMode::MaterialsAndTextures;
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
        .requirements = RenderFeatureRequirements{
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
    return registerScenePasses(builder, m_scene_state);
}

void Feature::prepareFrame(const RenderWorld& world,
                           const SceneRenderContext& scene_context,
                           const RenderFeatureFrameContext& frame_context,
                           RenderPassBlackboard& blackboard)
{
    namespace blackboard_names = luna::render_flow::blackboard;

    prepareResources(scene_context);

    blackboard_names::initializeSceneColorStageAliases(blackboard, scene_context.color_target);
    blackboard.set(blackboard_names::Depth, scene_context.depth_target);
    blackboard.set(blackboard_names::Pick, scene_context.pick_target);

    m_draw_queue.beginScene(world.camera());
    for (const auto& packet : world.drawPackets()) {
        m_draw_queue.submitDrawPacket(packet);
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
