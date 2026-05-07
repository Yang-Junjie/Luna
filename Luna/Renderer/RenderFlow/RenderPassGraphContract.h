#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderFlow/RenderPass.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace luna::render_flow {

struct RenderPassGraphContractPassInput {
    std::string_view pass_name;
    std::span<const RenderPassResourceUsage> resources;
};

struct RenderPassGraphContractFeatureInput {
    std::string_view feature_name;
    bool active{false};
    RenderFeatureRequirements requirements{};
    std::span<const RenderPassGraphContractPassInput> passes;
};

struct RenderPassGraphContractResult {
    std::string feature_name;
    bool valid{true};
    std::string summary;
};

[[nodiscard]] std::vector<RenderPassGraphContractResult>
    validateRenderPassGraphContracts(std::span<const RenderPassGraphContractFeatureInput> features);

} // namespace luna::render_flow
