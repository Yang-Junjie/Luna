#pragma once

#include "Renderer/VulkanRenderer.h"

#include <memory>

namespace luna::val {
class RenderGraph;
}

namespace luna::samples::model {

std::unique_ptr<luna::val::RenderGraph>
    buildModelRenderGraph(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info);

} // namespace luna::samples::model
