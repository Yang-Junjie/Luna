#include "EditorCamera.h"

#include "Core/Input.h"
#include "Events/Event.h"
#include "Events/MouseEvent.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace {

constexpr float kMinPitchRadians = -1.55334306f;
constexpr float kMaxPitchRadians = 1.55334306f;

} // namespace

namespace luna {

EditorCamera::EditorCamera()
{
    m_camera.setPerspective(glm::radians(50.0f), 0.05f, 500.0f);
    m_camera.setPosition(glm::vec3(0.0f, 0.75f, 6.0f));
    syncCameraRotation();
}

void EditorCamera::onUpdate(Timestep dt)
{
    const glm::vec2 current_mouse_position = Input::getMousePosition();
    if (!m_input_enabled || m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) {
        m_last_mouse_position = current_mouse_position;
        m_smoothed_velocity = glm::vec3(0.0f);
        endMouseCapture();
        return;
    }

    const bool wants_capture = Input::isMouseButtonPressed(MouseCode::Right);
    if (wants_capture && !m_mouse_captured) {
        beginMouseCapture();
    } else if (!wants_capture && m_mouse_captured) {
        endMouseCapture();
    }

    const glm::vec2 mouse_delta = m_mouse_captured ? (current_mouse_position - m_last_mouse_position) : glm::vec2(0.0f);
    m_last_mouse_position = current_mouse_position;

    if (m_mouse_captured) {
        m_yaw -= mouse_delta.x * m_mouse_sensitivity;
        m_pitch = glm::clamp(m_pitch + mouse_delta.y * m_mouse_sensitivity, kMinPitchRadians, kMaxPitchRadians);
        syncCameraRotation();
    }

    glm::vec3 input_axis(0.0f);
    if (m_mouse_captured) {
        if (Input::isKeyPressed(KeyCode::W)) {
            input_axis.z += 1.0f;
        }
        if (Input::isKeyPressed(KeyCode::S)) {
            input_axis.z -= 1.0f;
        }
        if (Input::isKeyPressed(KeyCode::D)) {
            input_axis.x += 1.0f;
        }
        if (Input::isKeyPressed(KeyCode::A)) {
            input_axis.x -= 1.0f;
        }
        if (Input::isKeyPressed(KeyCode::E)) {
            input_axis.y += 1.0f;
        }
        if (Input::isKeyPressed(KeyCode::Q)) {
            input_axis.y -= 1.0f;
        }
    }

    const glm::vec3 normalized_axis =
        glm::dot(input_axis, input_axis) > 0.0f ? glm::normalize(input_axis) : glm::vec3(0.0f);
    const glm::vec3 target_velocity = normalized_axis * currentMoveSpeed();
    const float blend = 1.0f - std::exp(-m_velocity_smoothing * (std::max)(dt.getSeconds(), 0.0f));
    m_smoothed_velocity = glm::mix(m_smoothed_velocity, target_velocity, blend);

    if (glm::dot(m_smoothed_velocity, m_smoothed_velocity) > 0.0f) {
        m_camera.translateLocal(m_smoothed_velocity * dt.getSeconds());
    }
}

void EditorCamera::onEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& mouse_event) {
        return onMouseScrolled(mouse_event);
    });
}

void EditorCamera::setViewportSize(float width, float height)
{
    m_viewport_size.x = (std::max)(width, 0.0f);
    m_viewport_size.y = (std::max)(height, 0.0f);
}

void EditorCamera::setInputEnabled(bool enabled)
{
    m_input_enabled = enabled;
    if (!m_input_enabled) {
        endMouseCapture();
    }
}

void EditorCamera::releaseMouseCapture()
{
    endMouseCapture();
}

bool EditorCamera::isMouseCaptured() const
{
    return m_mouse_captured;
}

Camera& EditorCamera::getCamera()
{
    return m_camera;
}

const Camera& EditorCamera::getCamera() const
{
    return m_camera;
}

bool EditorCamera::onMouseScrolled(MouseScrolledEvent& event)
{
    if (!m_input_enabled) {
        return false;
    }

    const float scroll_delta = event.getYOffset();
    if (scroll_delta == 0.0f) {
        return false;
    }

    m_camera.translateWorld(m_camera.getForwardDirection() * scroll_delta * m_scroll_step);
    return true;
}

void EditorCamera::beginMouseCapture()
{
    Input::setCursorMode(CursorMode::Locked);
    Input::setRawMouseMotion(true);
    m_last_mouse_position = Input::getMousePosition();
    m_mouse_captured = true;
}

void EditorCamera::endMouseCapture()
{
    if (!m_mouse_captured) {
        return;
    }

    Input::setCursorMode(CursorMode::Normal);
    Input::setRawMouseMotion(false);
    m_last_mouse_position = Input::getMousePosition();
    m_mouse_captured = false;
}

void EditorCamera::syncCameraRotation()
{
    m_camera.setYawPitchRoll(m_yaw, m_pitch);
}

float EditorCamera::currentMoveSpeed() const
{
    float speed = m_move_speed;
    if (Input::isKeyPressed(KeyCode::LeftShift) || Input::isKeyPressed(KeyCode::RightShift)) {
        speed *= m_fast_multiplier;
    }
    if (Input::isKeyPressed(KeyCode::LeftControl) || Input::isKeyPressed(KeyCode::RightControl)) {
        speed *= m_slow_multiplier;
    }
    return speed;
}

} // namespace luna
