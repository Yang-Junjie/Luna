#pragma once

#include "Core/application.h"
#include "Core/input.h"
#include "Core/layer.h"
#include "Vulkan/vk_loader.h"
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
        m_lastMousePosition = luna::Input::getMousePosition();
    }

    void onDetach() override
    {
        setCameraActive(false);
    }

    void onUpdate(Timestep dt) override
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool rightMouseHeld = luna::Input::isMouseButtonPressed(luna::MouseCode::Right);

        if (!m_cameraActive) {
            if (rightMouseHeld && !io.WantCaptureMouse) {
                setCameraActive(true);
            } else {
                m_lastMousePosition = luna::Input::getMousePosition();
                return;
            }
        } else if (!rightMouseHeld) {
            setCameraActive(false);
            return;
        }

        auto& renderService = luna::Application::get().getRenderService();
        auto& camera = renderService.getMainCamera();

        const glm::vec2 mousePosition = luna::Input::getMousePosition();
        const glm::vec2 mouseDelta = mousePosition - m_lastMousePosition;
        m_lastMousePosition = mousePosition;

        camera.yaw += mouseDelta.x * m_mouseSensitivity;
        camera.pitch -= mouseDelta.y * m_mouseSensitivity;
        camera.pitch = std::clamp(camera.pitch, -1.5f, 1.5f);

        const glm::mat4 cameraRotation = camera.get_rotation_matrix();
        const glm::vec3 forward = glm::normalize(glm::vec3(cameraRotation * glm::vec4(0, 0, -1, 0)));
        const glm::vec3 right = glm::normalize(glm::vec3(cameraRotation * glm::vec4(1, 0, 0, 0)));
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

        float speed = m_cameraSpeed;
        if (luna::Input::isKeyPressed(luna::KeyCode::LeftShift) ||
            luna::Input::isKeyPressed(luna::KeyCode::RightShift)) {
            speed *= m_cameraBoostMultiplier;
        }

        camera.position += movement * speed * static_cast<float>(dt);
    }

    void onImGuiRender() override
    {
        auto& renderService = luna::Application::get().getRenderService();
        auto& camera = renderService.getMainCamera();
        auto& effects = renderService.getBackgroundEffects();

        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &renderService.getRenderScale(), 0.3f, 1.0f);
            ImGui::Separator();

            ImGui::Text("Camera Active: %s", m_cameraActive ? "Yes" : "No");
            ImGui::Text("Position: %.2f %.2f %.2f",
                        camera.position.x,
                        camera.position.y,
                        camera.position.z);
            ImGui::Text("Pitch/Yaw: %.2f %.2f", camera.pitch, camera.yaw);
            ImGui::TextUnformatted("Hold RMB to look around. WASD move, Q/E vertical, Shift boost.");
            ImGui::Separator();

            if (effects.empty()) {
                ImGui::TextUnformatted("No background effects available.");
                ImGui::End();
                return;
            }

            auto& currentEffectIndex = renderService.getCurrentBackgroundEffect();
            const int lastEffectIndex = static_cast<int>(effects.size()) - 1;

            if (currentEffectIndex < 0) {
                currentEffectIndex = 0;
            }
            if (currentEffectIndex > lastEffectIndex) {
                currentEffectIndex = lastEffectIndex;
            }

            ImGui::Text("Selected effect: %s", effects[static_cast<size_t>(currentEffectIndex)].name);
            ImGui::SliderInt("Effect Index", &currentEffectIndex, 0, lastEffectIndex);

            ImGui::Separator();

            auto& selected = effects[static_cast<size_t>(currentEffectIndex)];

            ImGui::SliderFloat4("data1", &selected.data.data1.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data2", &selected.data.data2.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data3", &selected.data.data3.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data4", &selected.data.data4.x, 0.0f, 1.0f);
        }

        ImGui::End();

        if (ImGui::Begin("Scene")) {
            auto scene = renderService.findLoadedScene("basicmesh");
            if (!scene) {
                ImGui::TextUnformatted("basicmesh scene not loaded.");
            } else {
                std::vector<std::shared_ptr<Node>> editableNodes;
                editableNodes.reserve(3);

                for (const auto& topNode : scene->topNodes) {
                    collectEditableNodes(topNode, editableNodes, 3);
                    if (editableNodes.size() >= 3) {
                        break;
                    }
                }

                if (editableNodes.empty()) {
                    ImGui::TextUnformatted("No mesh nodes found.");
                } else {
                    ImGui::TextUnformatted("Adjust the first 3 mesh nodes in the glTF scene.");
                    ImGui::Separator();

                    for (size_t i = 0; i < editableNodes.size(); i++) {
                        auto& node = editableNodes[i];
                        if (!node) {
                            continue;
                        }

                        const std::string header =
                            node->name.empty() ? "Model " + std::to_string(i + 1)
                                               : node->name + "##model_" + std::to_string(i);

                        if (ImGui::TreeNode(header.c_str())) {
                            bool changed = false;

                            changed |= ImGui::DragFloat3(("Position##" + std::to_string(i)).c_str(),
                                                         &node->translation.x,
                                                         0.05f);
                            changed |= ImGui::DragFloat3(("Rotation##" + std::to_string(i)).c_str(),
                                                         &node->rotationEulerDegrees.x,
                                                         1.0f);
                            changed |= ImGui::DragFloat3(
                                ("Scale##" + std::to_string(i)).c_str(), &node->scale.x, 0.05f, 0.01f, 100.0f);

                            if (changed) {
                                node->scale.x = std::max(node->scale.x, 0.01f);
                                node->scale.y = std::max(node->scale.y, 0.01f);
                                node->scale.z = std::max(node->scale.z, 0.01f);
                                node->updateLocalTransform();
                            }

                            if (ImGui::Button(("Reset##" + std::to_string(i)).c_str())) {
                                node->translation = node->initialTranslation;
                                node->rotationEulerDegrees = node->initialRotationEulerDegrees;
                                node->scale = node->initialScale;
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
                                     std::vector<std::shared_ptr<Node>>& outNodes,
                                     size_t limit)
    {
        if (!node || outNodes.size() >= limit) {
            return;
        }

        if (dynamic_cast<MeshNode*>(node.get()) != nullptr) {
            outNodes.push_back(node);
            if (outNodes.size() >= limit) {
                return;
            }
        }

        for (const auto& child : node->children) {
            collectEditableNodes(child, outNodes, limit);
            if (outNodes.size() >= limit) {
                return;
            }
        }
    }

    void setCameraActive(bool active)
    {
        if (m_cameraActive == active) {
            return;
        }

        m_cameraActive = active;
        luna::Input::setCursorMode(active ? luna::CursorMode::Locked : luna::CursorMode::Normal);
        luna::Input::setRawMouseMotion(active);
        m_lastMousePosition = luna::Input::getMousePosition();
    }

private:
    bool m_cameraActive{false};
    glm::vec2 m_lastMousePosition{0.0f, 0.0f};
    float m_cameraSpeed{5.0f};
    float m_cameraBoostMultiplier{4.0f};
    float m_mouseSensitivity{0.003f};
};

} // namespace luna::editor
