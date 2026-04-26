#pragma once

#include <string_view>

namespace luna::render_flow::default_scene::pass_names {

inline constexpr std::string_view Geometry = "Geometry";
inline constexpr std::string_view Environment = "Environment";
inline constexpr std::string_view ShadowDepth = "ShadowDepth";
inline constexpr std::string_view Lighting = "Lighting";
inline constexpr std::string_view Transparent = "Transparent";

} // namespace luna::render_flow::default_scene::pass_names

namespace luna::render_flow::default_scene::blackboard {

inline constexpr std::string_view SceneColor = "DefaultScene.SceneColor";
inline constexpr std::string_view Depth = "DefaultScene.Depth";
inline constexpr std::string_view Pick = "DefaultScene.Pick";
inline constexpr std::string_view GBufferBaseColor = "DefaultScene.GBuffer.BaseColor";
inline constexpr std::string_view GBufferNormalMetallic = "DefaultScene.GBuffer.NormalMetallic";
inline constexpr std::string_view GBufferWorldPositionRoughness = "DefaultScene.GBuffer.WorldPositionRoughness";
inline constexpr std::string_view GBufferEmissiveAo = "DefaultScene.GBuffer.EmissiveAo";
inline constexpr std::string_view ShadowMap = "DefaultScene.ShadowMap";

} // namespace luna::render_flow::default_scene::blackboard
