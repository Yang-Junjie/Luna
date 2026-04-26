#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace luna {

[[nodiscard]] glm::mat4 flipProjectionY(glm::mat4 projection);
[[nodiscard]] float translationDistanceSquared(const glm::mat4& transform, const glm::vec3& position);

} // namespace luna
