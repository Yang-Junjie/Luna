#pragma once

#include "Core/Application.h"
#include "Core/Input.h"
#include "Core/Layer.h"
#include "imgui.h"

#include <algorithm>
#include <glm/glm.hpp>

namespace luna::editor {

class EditorCameraControllerLayer final : public Layer {
public:
    EditorCameraControllerLayer()
        : Layer("EditorCameraControllerLayer")
    {}

    void onAttach() override
    {
        m_last_mouse_position = luna::Input::getMousePosition();
    }

    void onDetach() override
    {
        setCameraActive(false);
    }

    void onUpdate(Timestep dt) override
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool right_mouse_held = luna::Input::isMouseButtonPressed(luna::MouseCode::Right);

        if (!m_camera_active) {
            if (right_mouse_held && !io.WantCaptureMouse) {
                setCameraActive(true);
            } else {
                m_last_mouse_position = luna::Input::getMousePosition();
                return;
            }
        } else if (!right_mouse_held) {
            setCameraActive(false);
            return;
        }

        auto& camera = luna::Application::get().getRenderer().getMainCamera();

        const glm::vec2 mouse_position = luna::Input::getMousePosition();
        const glm::vec2 mouse_delta = mouse_position - m_last_mouse_position;
        m_last_mouse_position = mouse_position;

        camera.m_yaw += mouse_delta.x * m_mouse_sensitivity;
        camera.m_pitch -= mouse_delta.y * m_mouse_sensitivity;
        camera.m_pitch = std::clamp(camera.m_pitch, -1.5f, 1.5f);

        const glm::mat4 camera_rotation = camera.getRotationMatrix();
        const glm::vec3 forward = glm::normalize(glm::vec3(camera_rotation * glm::vec4(0, 0, -1, 0)));
        const glm::vec3 right = glm::normalize(glm::vec3(camera_rotation * glm::vec4(1, 0, 0, 0)));
        const glm::vec3 up = glm::vec3(0, 1, 0);

        glm::vec3 movement{0.0f};
        if (luna::Input::isKeyPressed(luna::KeyCode::W)) {
            movement += forward;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::S)) {
            movement -= forward;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::D)) {
            movement += right;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::A)) {
            movement -= right;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::E)) {
            movement += up;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::Q)) {
            movement -= up;
        }

        if (glm::dot(movement, movement) > 0.0f) {
            movement = glm::normalize(movement);
        }

        float speed = m_camera_speed;
        if (luna::Input::isKeyPressed(luna::KeyCode::LeftShift) ||
            luna::Input::isKeyPressed(luna::KeyCode::RightShift)) {
            speed *= m_camera_boost_multiplier;
        }

        camera.m_position += movement * speed * static_cast<float>(dt);
    }

private:
    void setCameraActive(bool active)
    {
        if (m_camera_active == active) {
            return;
        }

        m_camera_active = active;
        luna::Input::setCursorMode(active ? luna::CursorMode::Locked : luna::CursorMode::Normal);
        luna::Input::setRawMouseMotion(active);
        m_last_mouse_position = luna::Input::getMousePosition();
    }

private:
    bool m_camera_active{false};
    glm::vec2 m_last_mouse_position{0.0f, 0.0f};
    float m_camera_speed{5.0f};
    float m_camera_boost_multiplier{4.0f};
    float m_mouse_sensitivity{0.003f};
};

} // namespace luna::editor
