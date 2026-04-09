#pragma once

#include "Renderer/VulkanRenderer.h"

#include <memory>

namespace VulkanAbstractionLayer {
class RenderGraph;
}

namespace luna::samples::texture {

std::unique_ptr<VulkanAbstractionLayer::RenderGraph> buildTextureRenderGraph(
    const luna::VulkanRenderer::RenderGraphBuildInfo& build_info);

} // namespace luna::samples::texture
