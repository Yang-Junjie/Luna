#pragma once

#include <string_view>

namespace luna::render_flow::default_scene::pass_names {

inline constexpr std::string_view Geometry = "Geometry";
inline constexpr std::string_view Lighting = "Lighting";
inline constexpr std::string_view Transparent = "Transparent";

} // namespace luna::render_flow::default_scene::pass_names
