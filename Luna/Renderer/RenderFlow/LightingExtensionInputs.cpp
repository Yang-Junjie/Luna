#include "Renderer/RenderFlow/LightingExtensionInputs.h"

#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderFlow/RenderResourceKey.h"
#include "Renderer/RenderGraphBuilder.h"

#include <array>

namespace luna::render_flow {
namespace {

constexpr std::array<LightingExtensionInput, kLightingExtensionInputCount> kLightingExtensionInputs{
    LightingExtensionInput::AmbientOcclusion,
    LightingExtensionInput::Reflection,
    LightingExtensionInput::IndirectDiffuse,
    LightingExtensionInput::IndirectSpecular,
};

constexpr size_t inputIndex(LightingExtensionInput input) noexcept
{
    return static_cast<size_t>(input);
}

constexpr RenderResourceKey<RenderGraphTextureHandle> inputKey(LightingExtensionInput input) noexcept
{
    switch (input) {
        case LightingExtensionInput::AmbientOcclusion:
            return {"Scene.LightingExtension.AmbientOcclusion"};
        case LightingExtensionInput::Reflection:
            return {"Scene.LightingExtension.Reflection"};
        case LightingExtensionInput::IndirectDiffuse:
            return {"Scene.LightingExtension.IndirectDiffuse"};
        case LightingExtensionInput::IndirectSpecular:
            return {"Scene.LightingExtension.IndirectSpecular"};
        case LightingExtensionInput::Count:
            break;
    }

    return {"Scene.LightingExtension.Invalid"};
}

[[nodiscard]] const luna::RHI::Ref<luna::RHI::Texture>&
    optionalTexture(RenderGraphRasterPassContext& pass_context, RenderGraphTextureHandle handle)
{
    return handle.isValid() ? pass_context.getTexture(handle) : pass_context.getTexture(RenderGraphTextureHandle{});
}

} // namespace

void LightingExtensionInputSet::set(LightingExtensionInput input, RenderGraphTextureHandle handle) noexcept
{
    const size_t index = inputIndex(input);
    if (index < textures.size()) {
        textures[index] = handle;
    }
}

RenderGraphTextureHandle LightingExtensionInputSet::get(LightingExtensionInput input) const noexcept
{
    const size_t index = inputIndex(input);
    return index < textures.size() ? textures[index] : RenderGraphTextureHandle{};
}

void setLightingExtensionInput(RenderPassBlackboard& blackboard,
                               LightingExtensionInput input,
                               RenderGraphTextureHandle handle)
{
    blackboard.set(inputKey(input), handle);
}

LightingExtensionInputSet getLightingExtensionInputs(const RenderPassBlackboard& blackboard)
{
    LightingExtensionInputSet inputs{};
    for (const LightingExtensionInput input : kLightingExtensionInputs) {
        inputs.set(input, blackboard.get(inputKey(input)).value_or(RenderGraphTextureHandle{}));
    }
    return inputs;
}

void readLightingExtensionInputTextures(RenderGraphRasterPassBuilder& pass_builder,
                                        const LightingExtensionInputSet& inputs)
{
    for (const RenderGraphTextureHandle handle : inputs.textures) {
        if (handle.isValid()) {
            pass_builder.ReadTexture(handle);
        }
    }
}

LightingExtensionTextureRefs resolveLightingExtensionInputTextures(RenderGraphRasterPassContext& pass_context,
                                                                   const LightingExtensionInputSet& inputs)
{
    return LightingExtensionTextureRefs{
        .ambient_occlusion = optionalTexture(pass_context, inputs.get(LightingExtensionInput::AmbientOcclusion)),
        .reflection = optionalTexture(pass_context, inputs.get(LightingExtensionInput::Reflection)),
        .indirect_diffuse = optionalTexture(pass_context, inputs.get(LightingExtensionInput::IndirectDiffuse)),
        .indirect_specular = optionalTexture(pass_context, inputs.get(LightingExtensionInput::IndirectSpecular)),
    };
}

} // namespace luna::render_flow
