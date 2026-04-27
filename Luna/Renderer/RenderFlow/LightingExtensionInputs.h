#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <Core.h>

namespace luna {
class RenderGraphRasterPassBuilder;
class RenderGraphRasterPassContext;
} // namespace luna

namespace luna::RHI {
class Texture;
} // namespace luna::RHI

namespace luna::render_flow {

class RenderPassBlackboard;

enum class LightingExtensionInput : uint8_t {
    AmbientOcclusion,
    Reflection,
    IndirectDiffuse,
    IndirectSpecular,
    Count,
};

inline constexpr size_t kLightingExtensionInputCount = static_cast<size_t>(LightingExtensionInput::Count);

struct LightingExtensionInputSet {
    std::array<RenderGraphTextureHandle, kLightingExtensionInputCount> textures{};

    void set(LightingExtensionInput input, RenderGraphTextureHandle handle) noexcept;
    [[nodiscard]] RenderGraphTextureHandle get(LightingExtensionInput input) const noexcept;
};

struct LightingExtensionTextureRefs {
    luna::RHI::Ref<luna::RHI::Texture> ambient_occlusion;
    luna::RHI::Ref<luna::RHI::Texture> reflection;
    luna::RHI::Ref<luna::RHI::Texture> indirect_diffuse;
    luna::RHI::Ref<luna::RHI::Texture> indirect_specular;
};

void setLightingExtensionInput(RenderPassBlackboard& blackboard,
                               LightingExtensionInput input,
                               RenderGraphTextureHandle handle);
[[nodiscard]] LightingExtensionInputSet getLightingExtensionInputs(const RenderPassBlackboard& blackboard);
void readLightingExtensionInputTextures(RenderGraphRasterPassBuilder& pass_builder,
                                        const LightingExtensionInputSet& inputs);
[[nodiscard]] LightingExtensionTextureRefs resolveLightingExtensionInputTextures(
    RenderGraphRasterPassContext& pass_context,
    const LightingExtensionInputSet& inputs);

} // namespace luna::render_flow
