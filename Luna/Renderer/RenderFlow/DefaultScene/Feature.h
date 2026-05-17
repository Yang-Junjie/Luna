#pragma once

#include "Renderer/Material.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderFlow/DefaultScene/AssetCache.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Environment.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/VisibilityBoundsOverlayPass.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/DefaultScene/SharedState.h"
#include "Renderer/RenderFlow/RenderFeature.h"

namespace luna::render_flow {
class RenderFlowBuilder;
class RenderPassBlackboard;
} // namespace luna::render_flow

namespace luna::render_flow::default_scene {

class Feature final : public luna::render_flow::IRenderFeature {
public:
    Feature();
    ~Feature() override;

    [[nodiscard]] RenderFeatureContract contract() const noexcept override;
    [[nodiscard]] std::vector<RenderFeatureParameterInfo> parameters() const override;
    [[nodiscard]] RenderFeatureDiagnostics diagnostics() const override;
    bool setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept override;
    bool registerPasses(RenderFlowBuilder& builder) override;
    void prepareFrame(const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      const RenderFeatureFrameContext& frame_context,
                      RenderPassBlackboard& blackboard) override;
    void shutdown() override;

private:
    void prepareResources(const SceneRenderContext& scene_context);

private:
    struct VisibilityDebugOptions {
        bool show_visible_bounds{false};
        bool show_culled_bounds{false};
        bool show_culling_frustum{false};
        bool freeze_culling_camera{false};
    };

    DrawQueue m_draw_queue{};
    DrawQueueStats m_last_draw_stats{};
    DrawQueueStats m_last_logged_draw_stats{};
    double m_last_draw_queue_cpu_ms{0.0};
    double m_last_draw_queue_avg_us_per_submitted_draw{0.0};
    VisibilityDebugStats m_last_visibility_debug_stats{};
    VisibilityDebugOptions m_visibility_debug{};
    Camera m_frozen_culling_camera{};
    float m_frozen_culling_aspect_ratio{1.0f};
    bool m_has_frozen_culling_camera{false};
    EnvironmentResources m_environment{};
    VisibilityBoundsOverlayResources m_visibility_bounds_overlay{};
    AssetCache m_assets{};
    PipelineResources m_pipelines{};
    Material m_default_material{};
    PassSharedState m_scene_state;
    bool m_has_logged_draw_stats{false};
    bool m_shutdown{false};
};

} // namespace luna::render_flow::default_scene
