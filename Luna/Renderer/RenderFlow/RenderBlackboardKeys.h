#pragma once

#include "Renderer/RenderFlow/RenderResourceKey.h"
#include "Renderer/RenderGraph.h"

#include <cstdint>

namespace luna::render_flow {
class RenderPassBlackboard;
}

namespace luna::render_flow::blackboard {

// Legacy external scene color target. During the staged migration this remains the physical editor/runtime
// color target, while the semantic stage keys below describe which pass owns each logical version.
inline constexpr RenderResourceKey<RenderGraphTextureHandle> SceneColor{"Scene.Color"};

// Scene color stage keys:
// - SceneLitColor: Lighting output before sky, temporal resolve, and transparent composition.
// - SceneSkyCompositedColor: Opaque lit color after sky/background composition.
// - SceneTemporalResolvedColor: Color after temporal AA/history resolve.
// - SceneTransparentCompositedColor: Color after transparent geometry composition.
// - SceneFinalColor: Final scene color handed back to the renderer output path.
inline constexpr RenderResourceKey<RenderGraphTextureHandle> SceneLitColor{"Scene.LitColor"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> SceneSkyCompositedColor{"Scene.SkyCompositedColor"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> SceneTemporalResolvedColor{"Scene.TemporalResolvedColor"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> SceneTransparentCompositedColor{
    "Scene.TransparentCompositedColor"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> SceneFinalColor{"Scene.FinalColor"};

enum class SceneColorStage : uint8_t {
    Lit,
    SkyComposited,
    TemporalResolved,
    TransparentComposited,
    Final,
};

[[nodiscard]] RenderResourceKey<RenderGraphTextureHandle> sceneColorStageKey(SceneColorStage stage) noexcept;
void initializeSceneColorStageAliases(RenderPassBlackboard& blackboard, RenderGraphTextureHandle external_color_target);
void publishSceneColorStage(RenderPassBlackboard& blackboard,
                            SceneColorStage stage,
                            RenderGraphTextureHandle color_handle);

inline constexpr RenderResourceKey<RenderGraphTextureHandle> Depth{"Scene.Depth"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> Pick{"Scene.Pick"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> GBufferBaseColor{"Scene.GBuffer.BaseColor"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> GBufferNormalMetallic{"Scene.GBuffer.NormalMetallic"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> GBufferWorldPositionRoughness{
    "Scene.GBuffer.WorldPositionRoughness"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> GBufferEmissiveAo{"Scene.GBuffer.EmissiveAo"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> Velocity{"Scene.Velocity"};
inline constexpr RenderResourceKey<RenderGraphTextureHandle> ShadowMap{"Scene.ShadowMap"};

} // namespace luna::render_flow::blackboard
