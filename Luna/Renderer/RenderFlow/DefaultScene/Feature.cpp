#include "Renderer/RenderFlow/DefaultScene/Feature.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/EnvironmentPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/GBufferPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/LightingPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/ShadowPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/TransparentPass.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderSlotPass.h"
#include "Renderer/RenderFlow/RenderSlots.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"

#include <memory>
#include <string>

namespace luna::render_flow::default_scene {
namespace {

bool registerScenePasses(RenderFlowBuilder& builder, PassSharedState& state)
{
    namespace pass_slots = luna::render_flow::slots::passes;
    namespace extension_slots = luna::render_flow::slots::extension_points;

    return builder.addPass(std::string(pass_slots::Environment),
                           std::make_unique<EnvironmentPass>(state)) &&
           builder.insertPassAfter(pass_slots::Environment,
                                   std::string(pass_slots::ShadowDepth),
                                   std::make_unique<ShadowDepthPass>(state)) &&
           builder.insertPassAfter(pass_slots::ShadowDepth,
                                   std::string(pass_slots::GBuffer),
                                   std::make_unique<GeometryPass>(state)) &&
           builder.insertPassAfter(pass_slots::GBuffer,
                                   std::string(extension_slots::AfterGBuffer),
                                   std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterGBuffer))) &&
           builder.insertPassAfter(extension_slots::AfterGBuffer,
                                   std::string(extension_slots::BeforeLighting),
                                   std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforeLighting))) &&
           builder.insertPassAfter(extension_slots::BeforeLighting,
                                   std::string(pass_slots::Lighting),
                                   std::make_unique<LightingPass>(state)) &&
           builder.insertPassAfter(pass_slots::Lighting,
                                   std::string(extension_slots::AfterLighting),
                                   std::make_unique<RenderSlotPass>(std::string(extension_slots::AfterLighting))) &&
           builder.insertPassAfter(extension_slots::AfterLighting,
                                   std::string(extension_slots::BeforeTransparent),
                                   std::make_unique<RenderSlotPass>(std::string(extension_slots::BeforeTransparent))) &&
           builder.insertPassAfter(extension_slots::BeforeTransparent,
                                   std::string(pass_slots::Transparent),
                                   std::make_unique<TransparentPass>(state)) &&
           builder.insertPassAfter(pass_slots::Transparent,
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

bool Feature::registerPasses(RenderFlowBuilder& builder)
{
    return registerScenePasses(builder, m_scene_state);
}

void Feature::prepareFrame(const RenderWorld& world,
                           const SceneRenderContext& scene_context,
                           RenderPassBlackboard& blackboard)
{
    namespace blackboard_names = luna::render_flow::blackboard;

    prepareResources(scene_context);

    blackboard.setTexture(blackboard_names::SceneColor, scene_context.color_target);
    blackboard.setTexture(blackboard_names::Depth, scene_context.depth_target);
    blackboard.setTexture(blackboard_names::Pick, scene_context.pick_target);

    m_draw_queue.beginScene(world.camera());
    for (const auto& packet : world.drawPackets()) {
        m_draw_queue.submitDrawPacket(packet);
    }
    m_scene_state.setWorld(world);
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
                           renderer_detail::backendTypeToString(scene_context.backend_type));
    } else if (invalidation == PipelineResources::Invalidation::MaterialsAndTextures) {
        LUNA_RENDERER_INFO("Rebuilding scene render flow pipeline state for backend '{}' and color format {} ({})",
                           renderer_detail::backendTypeToString(scene_context.backend_type),
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
