#pragma once

#include <string_view>

namespace luna::render_flow::blackboard {

inline constexpr std::string_view SceneColor = "Scene.Color";
inline constexpr std::string_view Depth = "Scene.Depth";
inline constexpr std::string_view Pick = "Scene.Pick";
inline constexpr std::string_view GBufferBaseColor = "Scene.GBuffer.BaseColor";
inline constexpr std::string_view GBufferNormalMetallic = "Scene.GBuffer.NormalMetallic";
inline constexpr std::string_view GBufferWorldPositionRoughness = "Scene.GBuffer.WorldPositionRoughness";
inline constexpr std::string_view GBufferEmissiveAo = "Scene.GBuffer.EmissiveAo";
inline constexpr std::string_view ShadowMap = "Scene.ShadowMap";

// Optional lighting extension inputs. Lighting uses neutral fallbacks when these are missing.
inline constexpr std::string_view AmbientOcclusion = "Scene.AmbientOcclusion";
inline constexpr std::string_view Reflection = "Scene.Reflection";
inline constexpr std::string_view IndirectDiffuse = "Scene.IndirectDiffuse";
inline constexpr std::string_view IndirectSpecular = "Scene.IndirectSpecular";

} // namespace luna::render_flow::blackboard
