#include "Core/Application.h"
#include "imgui.h"
#include "RendererInfoPanel.h"

namespace luna::editor {

void RendererInfoPanel::onImGuiRender()
{
    auto& renderer = luna::Application::get().getRenderer();

    ImGui::ColorEdit4("Clear Color", &renderer.getClearColor().x);
    ImGui::Separator();

    const auto& camera = renderer.getMainCamera();
    ImGui::Text("Position: %.2f %.2f %.2f", camera.m_position.x, camera.m_position.y, camera.m_position.z);
    ImGui::Text("Pitch/Yaw: %.2f %.2f", camera.m_pitch, camera.m_yaw);
    ImGui::TextUnformatted("Hold RMB to look around. WASD move, Q/E vertical, Shift boost.");
}

} // namespace luna::editor
