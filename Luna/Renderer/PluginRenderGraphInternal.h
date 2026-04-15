#pragma once

#include "Renderer/PluginRenderGraph.h"
#include "Renderer/VulkanRenderer.h"

namespace luna::render::detail {

VulkanRenderer::RenderGraphBuilderCallback createRenderGraphBuilderCallback(RenderGraphFactory factory, bool include_imgui);

} // namespace luna::render::detail
