#include "VkTypes.h"

#include <glm/vec3.hpp>

class Camera {
public:
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_position{0.0f};
    float m_pitch{0.0f};
    float m_yaw{0.0f};

    glm::mat4 getViewMatrix() const;
    glm::mat4 getRotationMatrix() const;
};

