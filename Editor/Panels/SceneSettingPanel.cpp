#include "SceneSettingPanel.h"

#include "EditorContext.h"
#include "EditorUI.h"

#include <cmath>
#include <glm/trigonometric.hpp>
#include <imgui.h>

namespace {

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameEnvironmentSettings(const luna::SceneEnvironmentSettings& lhs, const luna::SceneEnvironmentSettings& rhs)
{
    return lhs.backgroundMode == rhs.backgroundMode &&
           sameVec3(lhs.backgroundColor, rhs.backgroundColor) &&
           lhs.iblEnabled == rhs.iblEnabled &&
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

const char* backgroundModeLabel(luna::SceneBackgroundMode mode)
{
    switch (mode) {
        case luna::SceneBackgroundMode::SolidColor:
            return "Solid Color";
        case luna::SceneBackgroundMode::ProceduralSky:
            return "Default Sky";
        case luna::SceneBackgroundMode::EnvironmentMap:
            return "Environment Map";
    }

    return "Default Sky";
}

bool drawBackgroundModeCombo(luna::SceneBackgroundMode& mode)
{
    bool changed = false;
    if (ImGui::BeginCombo("Background", backgroundModeLabel(mode))) {
        const luna::SceneBackgroundMode modes[] = {
            luna::SceneBackgroundMode::SolidColor,
            luna::SceneBackgroundMode::ProceduralSky,
            luna::SceneBackgroundMode::EnvironmentMap,
        };

        for (const luna::SceneBackgroundMode candidate : modes) {
            const bool selected = mode == candidate;
            if (ImGui::Selectable(backgroundModeLabel(candidate), selected)) {
                mode = candidate;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

} // namespace

namespace luna {

SceneSettingPanel::SceneSettingPanel(EditorContext& editor_context)
    : m_editor_context(&editor_context)
{}

void SceneSettingPanel::syncFromScene()
{
    if (m_editor_context == nullptr) {
        return;
    }

    m_environment_draft = m_editor_context->getScene().environmentSettings();
    m_environment_draft.enabled = m_environment_draft.backgroundMode != SceneBackgroundMode::SolidColor;
    m_environment_draft_dirty = false;
    m_has_environment_draft = true;
}

void SceneSettingPanel::onImGuiRender()
{
    if (m_editor_context == nullptr) {
        return;
    }

    if (!m_has_environment_draft) {
        syncFromScene();
    }

    ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Settings");

    const SceneEnvironmentSettings& scene_environment = m_editor_context->getScene().environmentSettings();
    SceneEnvironmentSettings& environment = m_environment_draft;

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawBackgroundModeCombo(environment.backgroundMode);
        environment.enabled = environment.backgroundMode != SceneBackgroundMode::SolidColor;

        if (environment.backgroundMode == SceneBackgroundMode::SolidColor) {
            ImGui::ColorEdit3("Background Color", &environment.backgroundColor.x);
        }

        ImGui::Checkbox("IBL Enabled", &environment.iblEnabled);

        if (environment.backgroundMode == SceneBackgroundMode::EnvironmentMap ||
            environment.backgroundMode == SceneBackgroundMode::SolidColor) {
            editor::ui::drawAssetHandleEditor("Environment Map", environment.environmentMapHandle, {AssetType::Texture});
            if (ImGui::Button("Clear Environment Map", ImVec2(-1.0f, 0.0f))) {
                environment.environmentMapHandle = AssetHandle(0);
            }
        }

        ImGui::DragFloat("Intensity", &environment.intensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Sky Intensity", &environment.skyIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Diffuse Intensity", &environment.diffuseIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Specular Intensity", &environment.specularIntensity, 0.01f, 0.0f, 100.0f, "%.2f");

        if (environment.backgroundMode == SceneBackgroundMode::ProceduralSky) {
            ImGui::SeparatorText("Default Sky");
            editor::ui::drawVec3Control("Sun Direction",
                                         environment.proceduralSunDirection,
                                         0.0f,
                                         0.01f,
                                         editor::ui::PropertyLayout{120.0f});
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
        }

        m_environment_draft_dirty = !sameEnvironmentSettings(environment, scene_environment);

        ImGui::Separator();
        const bool disable_apply_controls = !m_environment_draft_dirty;
        if (disable_apply_controls) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f))) {
            m_environment_draft.enabled = m_environment_draft.backgroundMode != SceneBackgroundMode::SolidColor;
            m_editor_context->getScene().environmentSettings() = m_environment_draft;
            m_environment_draft_dirty = false;
            m_editor_context->markSceneDirty();
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
