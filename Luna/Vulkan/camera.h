#pragma once

#include "vk_types.h"

#include <glm/vec3.hpp>

class Camera {
public:
    glm::vec3 velocity{0.0f};
    glm::vec3 position{0.0f};
    float pitch{0.0f};
    float yaw{0.0f};

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_rotation_matrix() const;
};
