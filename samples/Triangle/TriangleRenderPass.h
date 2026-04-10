#pragma once

#include "Renderer/VulkanRenderer.h"

#include <memory>

namespace luna::val {
class RenderGraph;
}

namespace luna::samples::triangle {

std::unique_ptr<luna::val::RenderGraph>
    buildTriangleRenderGraph(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info);

} // namespace luna::samples::triangle
