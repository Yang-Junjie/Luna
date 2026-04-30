#include "SceneSettingPanel.h"

#include "Asset/AssetDatabase.h"
#include "EditorAssetDragDrop.h"
#include "LunaEditorLayer.h"

#include <algorithm>
#include <cmath>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <string>

namespace {

bool drawVec3Control(const std::string& label,
                     glm::vec3& values,
                     float reset_value = 0.0f,
                     float column_width = 120.0f,
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

std::string getAssetDisplayLabel(luna::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "None";
    }

    if (!luna::AssetDatabase::exists(handle)) {
        return "Unknown Asset";
    }

    const auto& metadata = luna::AssetDatabase::getAssetMetadata(handle);
    if (!metadata.Name.empty()) {
        return metadata.Name;
    }

    if (!metadata.FilePath.empty()) {
        return metadata.FilePath.generic_string();
    }

    return "Unnamed Asset";
}

bool drawAssetHandleEditor(const char* label,
                           luna::AssetHandle& handle,
                           std::initializer_list<luna::AssetType> accepted_types = {})
{
    bool changed = false;
    unsigned long long raw_handle = static_cast<unsigned long long>(static_cast<uint64_t>(handle));
    if (ImGui::InputScalar(label, ImGuiDataType_U64, &raw_handle)) {
        handle = luna::AssetHandle(static_cast<uint64_t>(raw_handle));
        changed = true;
    }

    if (ImGui::BeginDragDropTarget()) {
        luna::editor::AssetDragDropData payload{};
        if (luna::editor::acceptAssetDragDropPayload(payload, accepted_types)) {
            handle = luna::editor::getAssetHandle(payload);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    return changed;
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameEnvironmentSettings(const luna::SceneEnvironmentSettings& lhs, const luna::SceneEnvironmentSettings& rhs)
{
    return lhs.enabled == rhs.enabled && lhs.iblEnabled == rhs.iblEnabled &&
           lhs.environmentMapHandle == rhs.environmentMapHandle &&
           lhs.intensity == rhs.intensity && lhs.skyIntensity == rhs.skyIntensity &&
           lhs.diffuseIntensity == rhs.diffuseIntensity && lhs.specularIntensity == rhs.specularIntensity &&
           sameVec3(lhs.proceduralSunDirection, rhs.proceduralSunDirection) &&
           lhs.proceduralSunIntensity == rhs.proceduralSunIntensity &&
           lhs.proceduralSunAngularRadius == rhs.proceduralSunAngularRadius &&
           sameVec3(lhs.proceduralSkyColorZenith, rhs.proceduralSkyColorZenith) &&
           sameVec3(lhs.proceduralSkyColorHorizon, rhs.proceduralSkyColorHorizon) &&
           sameVec3(lhs.proceduralGroundColor, rhs.proceduralGroundColor) &&
           lhs.proceduralSkyExposure == rhs.proceduralSkyExposure;
}

} // namespace

namespace luna {

SceneSettingPanel::SceneSettingPanel(LunaEditorLayer& editor_layer)
    : m_editor_layer(&editor_layer)
{}

void SceneSettingPanel::syncFromScene()
{
    if (m_editor_layer == nullptr) {
        return;
    }

    m_environment_draft = m_editor_layer->getScene().environmentSettings();
    m_environment_draft_dirty = false;
    m_has_environment_draft = true;
}

void SceneSettingPanel::onImGuiRender()
{
    if (m_editor_layer == nullptr) {
        return;
    }

    if (!m_has_environment_draft) {
        syncFromScene();
    }

    ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Settings");

    const SceneEnvironmentSettings& scene_environment = m_editor_layer->getScene().environmentSettings();
    SceneEnvironmentSettings& environment = m_environment_draft;
    m_environment_draft_dirty = !sameEnvironmentSettings(environment, scene_environment);

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", &environment.enabled);
        ImGui::Checkbox("IBL Enabled", &environment.iblEnabled);

        drawAssetHandleEditor("Environment Map", environment.environmentMapHandle, {AssetType::Texture});
        ImGui::TextDisabled("Environment Asset: %s", getAssetDisplayLabel(environment.environmentMapHandle).c_str());
        if (ImGui::Button("Clear Environment Map", ImVec2(-1.0f, 0.0f))) {
            environment.environmentMapHandle = AssetHandle(0);
        }

        ImGui::DragFloat("Intensity", &environment.intensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Sky Intensity", &environment.skyIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Diffuse Intensity", &environment.diffuseIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Specular Intensity", &environment.specularIntensity, 0.01f, 0.0f, 100.0f, "%.2f");

        ImGui::SeparatorText("Default Sky");
        drawVec3Control("Sun Direction", environment.proceduralSunDirection, 0.0f, 120.0f, 0.01f);
        ImGui::DragFloat("Sun Intensity", &environment.proceduralSunIntensity, 0.05f, 0.0f, 1000.0f, "%.2f");
        ImGui::DragFloat("Sun Angular Radius",
                         &environment.proceduralSunAngularRadius,
                         0.001f,
                         0.0f,
                         0.25f,
                         "%.4f");
        ImGui::ColorEdit3("Sky Zenith", &environment.proceduralSkyColorZenith.x);
        ImGui::ColorEdit3("Sky Horizon", &environment.proceduralSkyColorHorizon.x);
        ImGui::ColorEdit3("Ground", &environment.proceduralGroundColor.x);
        ImGui::DragFloat("Sky Exposure", &environment.proceduralSkyExposure, 0.01f, 0.0f, 100.0f, "%.2f");

        ImGui::Separator();
        const bool disable_apply_controls = !m_environment_draft_dirty;
        if (disable_apply_controls) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f))) {
            m_editor_layer->getScene().environmentSettings() = m_environment_draft;
            m_environment_draft_dirty = false;
            m_editor_layer->markSceneDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert", ImVec2(120.0f, 0.0f))) {
            syncFromScene();
        }
        if (disable_apply_controls) {
            ImGui::EndDisabled();
        }

        if (m_environment_draft_dirty) {
            ImGui::TextDisabled("Environment changes are pending.");
        }
    }

    ImGui::End();
}

} // namespace luna
