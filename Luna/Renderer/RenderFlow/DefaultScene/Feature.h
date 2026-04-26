#pragma once

#include "Renderer/Material.h"
#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Environment.h"
#include "Renderer/RenderFlow/DefaultScene/SceneAssetResources.h"
#include "Renderer/RenderFlow/DefaultScene/ScenePipelineResources.h"
#include "Renderer/RenderFlow/DefaultScene/SharedState.h"

namespace luna::render_flow {
class RenderFlowBuilder;
class RenderPassBlackboard;
} // namespace luna::render_flow

namespace luna::render_flow::default_scene {

class Feature final : public luna::render_flow::IRenderFeature {
public:
    Feature();
    ~Feature() override;

    bool registerPasses(RenderFlowBuilder& builder) override;
    void prepareFrame(const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      RenderPassBlackboard& blackboard) override;
    void shutdown() override;

private:
    void prepareResources(const SceneRenderContext& scene_context);

private:
    DrawQueue m_draw_queue{};
    EnvironmentResources m_environment{};
    SceneAssetResources m_assets{};
    ScenePipelineResources m_pipelines{};
    Material m_default_material{};
    PassSharedState m_scene_state;
    bool m_shutdown{false};
};

} // namespace luna::render_flow::default_scene
