#pragma once

#include "Core/Application.h"
#include "Core/Input.h"
#include "Core/Layer.h"
#include "Core/MouseCodes.h"
#include "imgui.h"

#include <algorithm>
#include <string>

namespace luna::samples {

struct SampleCameraLayerOptions {
    std::string m_title;
    std::string m_description;
    glm::vec3 m_initial_position{0.0f, 0.0f, 3.0f};
    float m_camera_speed{5.0f};
    float m_camera_boost_multiplier{4.0f};
    float m_mouse_sensitivity{0.003f};
};

class SampleCameraLayer final : public Layer {
public:
    explicit SampleCameraLayer(SampleCameraLayerOptions options)
        : Layer("SampleCameraLayer"),
          m_options(std::move(options))
    {}

    void onAttach() override
    {
        auto& camera = Application::get().getRenderer().getMainCamera();
        camera.m_position = m_options.m_initial_position;
        camera.m_pitch = 0.0f;
        camera.m_yaw = 0.0f;
        m_last_mouse_position = Input::getMousePosition();
    }

    void onDetach() override
    {
        setCameraActive(false);
    }

    void onUpdate(Timestep dt) override
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool right_mouse_held = Input::isMouseButtonPressed(MouseCode::Right);

        if (!m_camera_active) {
            if (right_mouse_held && !io.WantCaptureMouse) {
                setCameraActive(true);
            } else {
                m_last_mouse_position = Input::getMousePosition();
                return;
            }
        } else if (!right_mouse_held) {
            setCameraActive(false);
            return;
        }

        auto& camera = Application::get().getRenderer().getMainCamera();

        const glm::vec2 mouse_position = Input::getMousePosition();
        const glm::vec2 mouse_delta = mouse_position - m_last_mouse_position;
        m_last_mouse_position = mouse_position;

        camera.m_yaw += mouse_delta.x * m_options.m_mouse_sensitivity;
        camera.m_pitch -= mouse_delta.y * m_options.m_mouse_sensitivity;
        camera.m_pitch = std::clamp(camera.m_pitch, -1.5f, 1.5f);

        const glm::mat4 camera_rotation = camera.getRotationMatrix();
        const glm::vec3 forward = glm::normalize(glm::vec3(camera_rotation * glm::vec4(0, 0, -1, 0)));
        const glm::vec3 right = glm::normalize(glm::vec3(camera_rotation * glm::vec4(1, 0, 0, 0)));
        const glm::vec3 up = glm::vec3(0, 1, 0);

        glm::vec3 movement{0.0f};
        if (Input::isKeyPressed(KeyCode::W)) {
            movement += forward;
        }
        if (Input::isKeyPressed(KeyCode::S)) {
            movement -= forward;
        }
        if (Input::isKeyPressed(KeyCode::D)) {
            movement += right;
        }
        if (Input::isKeyPressed(KeyCode::A)) {
            movement -= right;
        }
        if (Input::isKeyPressed(KeyCode::E)) {
            movement += up;
        }
        if (Input::isKeyPressed(KeyCode::Q)) {
            movement -= up;
        }

        if (glm::dot(movement, movement) > 0.0f) {
            movement = glm::normalize(movement);
        }

        float speed = m_options.m_camera_speed;
        if (Input::isKeyPressed(KeyCode::LeftShift) || Input::isKeyPressed(KeyCode::RightShift)) {
            speed *= m_options.m_camera_boost_multiplier;
        }

        camera.m_position += movement * speed * static_cast<float>(dt);
    }

    void onImGuiRender() override
    {
        auto& renderer = Application::get().getRenderer();
        const auto& camera = renderer.getMainCamera();

        if (ImGui::Begin("Sample")) {
            ImGui::TextUnformatted(m_options.m_title.c_str());
            if (!m_options.m_description.empty()) {
                ImGui::Separator();
                ImGui::TextWrapped("%s", m_options.m_description.c_str());
            }
            ImGui::Separator();
            ImGui::Text("Camera Active: %s", m_camera_active ? "Yes" : "No");
            ImGui::Text("Position: %.2f %.2f %.2f", camera.m_position.x, camera.m_position.y, camera.m_position.z);
            ImGui::Text("Pitch/Yaw: %.2f %.2f", camera.m_pitch, camera.m_yaw);
            ImGui::TextUnformatted("Hold RMB to look around. WASD move, Q/E vertical, Shift boost.");
        }
        ImGui::End();
    }

private:
    void setCameraActive(bool active)
    {
        if (m_camera_active == active) {
            return;
        }

        m_camera_active = active;
        Input::setCursorMode(active ? CursorMode::Locked : CursorMode::Normal);
        Input::setRawMouseMotion(active);
        m_last_mouse_position = Input::getMousePosition();
    }

private:
    SampleCameraLayerOptions m_options;
    bool m_camera_active{false};
    glm::vec2 m_last_mouse_position{0.0f, 0.0f};
};

} // namespace luna::samples
