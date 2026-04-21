#pragma once

#include "Core/Timestep.h"
#include "Renderer/Camera.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace luna {

class Event;
class MouseScrolledEvent;

class EditorCamera {
public:
    EditorCamera();

    void onUpdate(Timestep dt);
    void onEvent(Event& event);

    void setViewportSize(float width, float height);
    void setInputEnabled(bool enabled);
    void releaseMouseCapture();

    bool isMouseCaptured() const;

    Camera& getCamera();
    const Camera& getCamera() const;

private:
    bool onMouseScrolled(MouseScrolledEvent& event);
    void beginMouseCapture();
    void endMouseCapture();
    void syncCameraRotation();
    float currentMoveSpeed() const;

private:
    Camera m_camera;
    glm::vec2 m_viewport_size{1.0f, 1.0f};
    glm::vec2 m_last_mouse_position{0.0f, 0.0f};
    glm::vec3 m_smoothed_velocity{0.0f, 0.0f, 0.0f};
    float m_yaw{0.0f};
    float m_pitch{0.0f};
    float m_move_speed{6.0f};
    float m_fast_multiplier{3.0f};
    float m_slow_multiplier{0.35f};
    float m_mouse_sensitivity{0.0025f};
    float m_scroll_step{1.5f};
    float m_velocity_smoothing{14.0f};
    bool m_input_enabled{false};
    bool m_mouse_captured{false};
};

} // namespace luna
