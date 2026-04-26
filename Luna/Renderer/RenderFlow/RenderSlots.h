#pragma once

#include <string_view>

namespace luna::render_flow::slots {

namespace passes {

inline constexpr std::string_view Environment = "Environment";
inline constexpr std::string_view ShadowDepth = "ShadowDepth";
inline constexpr std::string_view GBuffer = "GBuffer";
inline constexpr std::string_view Lighting = "Lighting";
inline constexpr std::string_view Transparent = "Transparent";

} // namespace passes

namespace extension_points {

inline constexpr std::string_view AfterGBuffer = "AfterGBuffer";
inline constexpr std::string_view BeforeLighting = "BeforeLighting";
inline constexpr std::string_view AfterLighting = "AfterLighting";
inline constexpr std::string_view BeforeTransparent = "BeforeTransparent";
inline constexpr std::string_view AfterTransparent = "AfterTransparent";

} // namespace extension_points

} // namespace luna::render_flow::slots
