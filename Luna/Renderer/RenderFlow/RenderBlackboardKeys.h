#pragma once

#include "Renderer/RenderFlow/RenderResourceKey.h"
#include "Renderer/RenderGraph.h"

namespace luna::render_flow::blackboard {

inline constexpr RenderResourceKey<RenderGraphTextureHandle> SceneColor{"Scene.Color"};
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
