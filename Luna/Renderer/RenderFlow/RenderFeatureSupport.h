#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"

#include <string>
#include <vector>

namespace luna::render_flow {

struct RenderFeatureSupportResult {
    bool supported{true};
    std::vector<std::string> reasons;
    std::vector<std::string> deferred_checks;
};

[[nodiscard]] RenderFeatureSupportResult
    evaluateRenderFeatureRequirements(const RenderFeatureRequirements& requirements,
                                      const SceneRenderContext& scene_context);

[[nodiscard]] RenderFeatureSupportResult evaluateRenderFeatureSupport(const IRenderFeature& feature,
                                                                      const SceneRenderContext& scene_context);

[[nodiscard]] std::string summarizeRenderFeatureSupport(const RenderFeatureSupportResult& result);

} // namespace luna::render_flow
