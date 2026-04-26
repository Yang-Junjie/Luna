#include "Math/Math.h"

#include <glm/gtx/norm.hpp>

namespace luna {

glm::mat4 flipProjectionY(glm::mat4 projection)
{
    projection[1][1] *= -1.0f;
    return projection;
}

float translationDistanceSquared(const glm::mat4& transform, const glm::vec3& position)
{
    const glm::vec3 translation(transform[3]);
    return glm::length2(translation - position);
}

} // namespace luna
