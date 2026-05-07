#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace luna::render_flow {

struct RenderFeatureGraphContractInput {
    std::string_view feature_name;
    bool active{false};
    RenderFeatureRequirements requirements{};
};

struct RenderFeatureGraphContractResult {
    std::string feature_name;
    bool valid{true};
    std::string summary;
};

[[nodiscard]] std::vector<RenderFeatureGraphContractResult>
    validateRenderFeatureGraphContracts(std::span<const RenderFeatureGraphContractInput> features);

} // namespace luna::render_flow
