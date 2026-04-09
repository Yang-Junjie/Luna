#pragma once

#include "Renderer/VulkanRenderer.h"

#include <memory>

namespace VulkanAbstractionLayer {
class RenderGraph;
}

namespace luna::samples::triangle {

std::unique_ptr<VulkanAbstractionLayer::RenderGraph> buildTriangleRenderGraph(
    const luna::VulkanRenderer::RenderGraphBuildInfo& build_info);

} // namespace luna::samples::triangle
