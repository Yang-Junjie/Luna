#pragma once

#include <cstdint>

#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

namespace luna {

struct LightComponent {
    enum class Type : uint8_t {
        Directional = 0,
        Point = 1,
        Spot = 2,
    };

    Type type = Type::Directional;
    bool enabled = true;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 4.0f;
    float range = 10.0f;
    float innerConeAngleRadians = glm::radians(20.0f);
    float outerConeAngleRadians = glm::radians(35.0f);

    LightComponent() = default;
    LightComponent(const LightComponent&) = default;
};

} // namespace luna
