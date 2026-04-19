#include "InspectorPanel.h"
#include "LunaEditorApp.h"
#include "Scene/Components.h"

#include <algorithm>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <string>

namespace {

bool drawVec3Control(const std::string& label,
                     glm::vec3& values,
                     float reset_value = 0.0f,
                     float column_width = 100.0f,
                     float drag_speed = 0.1f)
{
    bool changed = false;
    ImGui::PushID(label.c_str());
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{0.0f, 1.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4.0f, 1.0f});

    if (ImGui::BeginTable("##Vec3Table", 2, ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, column_width);
        ImGui::TableSetupColumn("##controls", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFontSize() + 2.0f);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label.c_str());

        ImGui::TableNextColumn();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4.0f, 0.0f});

        const float line_height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        const ImVec2 button_size{line_height + 3.0f, line_height};
        const float item_spacing = ImGui::GetStyle().ItemSpacing.x;
        const float item_width =
            (std::max)((ImGui::GetContentRegionAvail().x - button_size.x * 3.0f - item_spacing * 2.0f) / 3.0f, 1.0f);

        auto draw_axis_control =
            [&](const char* axis_label, float& value, const ImVec4& color, const ImVec4& hovered_color, bool last) {
                bool axis_changed = false;

                ImGui::PushStyleColor(ImGuiCol_Button, color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
                if (ImGui::Button(axis_label, button_size)) {
                    value = reset_value;
                    axis_changed = true;
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::SetNextItemWidth(item_width);
                if (ImGui::DragFloat(
                        (std::string("##") + axis_label).c_str(), &value, drag_speed, 0.0f, 0.0f, "%.2f")) {
                    axis_changed = true;
                }

                if (!last) {
                    ImGui::SameLine();
                }

                return axis_changed;
            };

        changed |= draw_axis_control(
            "X", values.x, ImVec4{0.80f, 0.10f, 0.15f, 1.0f}, ImVec4{0.90f, 0.20f, 0.20f, 1.0f}, false);
        changed |= draw_axis_control(
            "Y", values.y, ImVec4{0.20f, 0.70f, 0.20f, 1.0f}, ImVec4{0.30f, 0.80f, 0.30f, 1.0f}, false);
        changed |= draw_axis_control(
            "Z", values.z, ImVec4{0.10f, 0.25f, 0.80f, 1.0f}, ImVec4{0.20f, 0.35f, 0.90f, 1.0f}, true);

        ImGui::PopStyleVar();
        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return changed;
}

} // namespace

namespace luna {

InspectorPanel::InspectorPanel(LunaEditorApplication& application)
    : m_application(&application)
{}

void InspectorPanel::onImGuiRender()
{
    if (m_application == nullptr) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    Entity selected_entity = m_application->getSelectedEntity();
    if (!selected_entity) {
        ImGui::TextUnformatted("Select an entity to inspect.");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity: %s", selected_entity.getName().c_str());
    ImGui::Separator();

    if (selected_entity.hasComponent<TransformComponent>()) {
        auto& transform = selected_entity.getComponent<TransformComponent>();
        drawVec3Control("Translation", transform.translation, 0.0f);

        glm::vec3 rotation_degrees = glm::degrees(transform.rotation);
        if (drawVec3Control("Rotation", rotation_degrees, 0.0f)) {
            transform.rotation = glm::radians(rotation_degrees);
        }

        drawVec3Control("Scale", transform.scale, 1.0f);
        ImGui::Spacing();
    } else {
        ImGui::TextUnformatted("Selected entity does not have a TransformComponent.");
    }

    ImGui::End();
}

} // namespace luna
