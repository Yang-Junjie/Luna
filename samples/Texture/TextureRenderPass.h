#pragma once

#include "Renderer/VulkanRenderer.h"

#include <memory>

namespace luna::val {
class RenderGraph;
}

namespace luna::samples::texture {

std::unique_ptr<luna::val::RenderGraph>
    buildTextureRenderGraph(const luna::VulkanRenderer::RenderGraphBuildInfo& build_info);

} // namespace luna::samples::texture
