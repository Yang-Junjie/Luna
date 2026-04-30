#pragma once

#include "Renderer/Camera.h"

#include <glm/trigonometric.hpp>

namespace luna {

struct CameraComponent {
    bool primary = true;
    bool fixedAspectRatio = false;
    Camera::ProjectionType projectionType = Camera::ProjectionType::Perspective;
    float perspectiveVerticalFovRadians = glm::radians(50.0f);
    float perspectiveNear = 0.05f;
    float perspectiveFar = 500.0f;
    float orthographicSize = 10.0f;
    float orthographicNear = -100.0f;
    float orthographicFar = 100.0f;

    CameraComponent() = default;
    CameraComponent(const CameraComponent&) = default;

    Camera createCamera() const
    {
        Camera camera;
        if (projectionType == Camera::ProjectionType::Orthographic) {
            camera.setOrthographic(orthographicSize, orthographicNear, orthographicFar);
        } else {
            camera.setPerspective(perspectiveVerticalFovRadians, perspectiveNear, perspectiveFar);
        }
        return camera;
    }
};

} // namespace luna
