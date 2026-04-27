#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

namespace luna::render_flow {

// Validation-only feature used to prove external features can write public lighting inputs
// without depending on DefaultScene internals.
class FeatureProbe final : public IRenderFeature {
public:
    [[nodiscard]] RenderFeatureInfo info() const noexcept override;
    bool registerPasses(RenderFlowBuilder& builder) override;
    void prepareFrame(const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      RenderPassBlackboard& blackboard) override;
    void shutdown() override;
};

} // namespace luna::render_flow
