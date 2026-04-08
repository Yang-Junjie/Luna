#pragma once

#include "Core/Application.h"
#include "Core/Input.h"
#include "Core/Layer.h"
#include "Vulkan/VkLoader.h"
#include "imgui.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace luna::editor {

class EditorLayer final : public Layer {
public:
    EditorLayer()
        : Layer("EditorLayer")
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

        auto& camera = luna::Application::get().getEngine().m_main_camera;

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

    void onImGuiRender() override
    {
        auto& engine = luna::Application::get().getEngine();
        auto& effects = engine.getBackgroundEffects();

        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &engine.m_render_scale, 0.3f, 1.0f);
            ImGui::Separator();

            ImGui::Text("Camera Active: %s", m_camera_active ? "Yes" : "No");
            ImGui::Text("Position: %.2f %.2f %.2f",
                        engine.m_main_camera.m_position.x,
                        engine.m_main_camera.m_position.y,
                        engine.m_main_camera.m_position.z);
            ImGui::Text("Pitch/Yaw: %.2f %.2f", engine.m_main_camera.m_pitch, engine.m_main_camera.m_yaw);
            ImGui::TextUnformatted("Hold RMB to look around. WASD move, Q/E vertical, Shift boost.");
            ImGui::Separator();

            if (effects.empty()) {
                ImGui::TextUnformatted("No background effects available.");
                ImGui::End();
                return;
            }

            auto& current_effect_index = engine.getCurrentBackgroundEffect();
            const int last_effect_index = static_cast<int>(effects.size()) - 1;

            if (current_effect_index < 0) {
                current_effect_index = 0;
            }
            if (current_effect_index > last_effect_index) {
                current_effect_index = last_effect_index;
            }

            ImGui::Text("Selected effect: %s", effects[static_cast<size_t>(current_effect_index)].m_name);
            ImGui::SliderInt("Effect Index", &current_effect_index, 0, last_effect_index);

            ImGui::Separator();

            auto& selected = effects[static_cast<size_t>(current_effect_index)];

            ImGui::SliderFloat4("data1", &selected.m_data.m_data1.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data2", &selected.m_data.m_data2.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data3", &selected.m_data.m_data3.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data4", &selected.m_data.m_data4.x, 0.0f, 1.0f);
        }

        ImGui::End();

        if (ImGui::Begin("Scene")) {
            auto scene_it = engine.m_loaded_scenes.find("basicmesh");
            if (scene_it == engine.m_loaded_scenes.end() || !scene_it->second) {
                ImGui::TextUnformatted("basicmesh scene not loaded.");
            } else {
                std::vector<std::shared_ptr<Node>> editable_nodes;
                editable_nodes.reserve(3);

                for (const auto& top_node : scene_it->second->m_top_nodes) {
                    collectEditableNodes(top_node, editable_nodes, 3);
                    if (editable_nodes.size() >= 3) {
                        break;
                    }
                }

                if (editable_nodes.empty()) {
                    ImGui::TextUnformatted("No mesh nodes found.");
                } else {
                    ImGui::TextUnformatted("Adjust the first 3 mesh nodes in the glTF scene.");
                    ImGui::Separator();

                    for (size_t i = 0; i < editable_nodes.size(); i++) {
                        auto& node = editable_nodes[i];
                        if (!node) {
                            continue;
                        }

                        const std::string header =
                            node->m_name.empty() ? "Model " + std::to_string(i + 1)
                                                 : node->m_name + "##model_" + std::to_string(i);

                        if (ImGui::TreeNode(header.c_str())) {
                            bool changed = false;

                            changed |= ImGui::DragFloat3(("Position##" + std::to_string(i)).c_str(),
                                                         &node->m_translation.x,
                                                         0.05f);
                            changed |= ImGui::DragFloat3(("Rotation##" + std::to_string(i)).c_str(),
                                                         &node->m_rotation_euler_degrees.x,
                                                         1.0f);
                            changed |= ImGui::DragFloat3(
                                ("Scale##" + std::to_string(i)).c_str(), &node->m_scale.x, 0.05f, 0.01f, 100.0f);

                            if (changed) {
                                node->m_scale.x = std::max(node->m_scale.x, 0.01f);
                                node->m_scale.y = std::max(node->m_scale.y, 0.01f);
                                node->m_scale.z = std::max(node->m_scale.z, 0.01f);
                                node->updateLocalTransform();
                            }

                            if (ImGui::Button(("Reset##" + std::to_string(i)).c_str())) {
                                node->m_translation = node->m_initial_translation;
                                node->m_rotation_euler_degrees = node->m_initial_rotation_euler_degrees;
                                node->m_scale = node->m_initial_scale;
                                node->updateLocalTransform();
                            }

                            ImGui::TreePop();
                        }
                    }
                }
            }
        }

        ImGui::End();
    }

private:
    static void collectEditableNodes(const std::shared_ptr<Node>& node,
                                     std::vector<std::shared_ptr<Node>>& out_nodes,
                                     size_t limit)
    {
        if (!node || out_nodes.size() >= limit) {
            return;
        }

        if (dynamic_cast<MeshNode*>(node.get()) != nullptr) {
            out_nodes.push_back(node);
            if (out_nodes.size() >= limit) {
                return;
            }
        }

        for (const auto& child : node->m_children) {
            collectEditableNodes(child, out_nodes, limit);
            if (out_nodes.size() >= limit) {
                return;
            }
        }
    }

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

