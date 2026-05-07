#pragma once

#include "Renderer/RenderFlow/DefaultScene/AssetCache.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/PassResources.h"
#include "Renderer/RenderFlow/DefaultScene/SharedState.h"
#include "Renderer/RenderFlow/RenderPass.h"

#include <cstddef>

#include <string_view>
#include <vector>

namespace luna {
class Material;
}

namespace luna::RHI {
class CommandBufferEncoder;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

void configureViewportAndScissor(luna::RHI::CommandBufferEncoder& commands, uint32_t width, uint32_t height);

[[nodiscard]] size_t recordDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                                        const DrawPassResources& pass_resources,
                                        const std::vector<DrawCommand>& draw_commands,
                                        const AssetCache& assets,
                                        const Material& default_material);

[[nodiscard]] size_t recordShadowDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                                              const DrawPassResources& pass_resources,
                                              const std::vector<DrawCommand>& draw_commands,
                                              const AssetCache& assets,
                                              const Material& default_material,
                                              uint32_t cascade_index = 0);

void updateSceneParameters(PassSharedState& state, const SceneRenderContext& context);
void updateEnvironmentBindings(PassSharedState& state);

[[nodiscard]] RenderGraphTextureHandle readBlackboardTexture(const RenderPassBlackboard& blackboard,
                                                             RenderResourceKey<RenderGraphTextureHandle> key,
                                                             std::string_view pass_name);

} // namespace luna::render_flow::default_scene
