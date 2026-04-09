#pragma once

#include "Renderer/VulkanRenderer.h"

#include <memory>

namespace VulkanAbstractionLayer {
class RenderGraph;
}

namespace luna::samples::model {

std::unique_ptr<VulkanAbstractionLayer::RenderGraph> buildModelRenderGraph(
    const luna::VulkanRenderer::RenderGraphBuildInfo& build_info);

} // namespace luna::samples::model
