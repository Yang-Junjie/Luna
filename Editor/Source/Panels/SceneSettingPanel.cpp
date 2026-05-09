#include "SceneSettingPanel.h"

#include "EditorContext.h"
#include "EditorUI.h"

#include <algorithm>
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

bool sameShadowSettings(const luna::SceneShadowSettings& lhs, const luna::SceneShadowSettings& rhs)
{
    return lhs.mode == rhs.mode &&
           lhs.pcfShadowDistance == rhs.pcfShadowDistance &&
           lhs.pcfMapSize == rhs.pcfMapSize &&
           lhs.csmCascadeSize == rhs.csmCascadeSize;
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

const char* shadowModeLabel(luna::SceneShadowMode mode)
{
    switch (mode) {
        case luna::SceneShadowMode::None:
            return "None";
        case luna::SceneShadowMode::PcfShadowMap:
            return "PCF Shadow Map";
        case luna::SceneShadowMode::CascadedShadowMaps:
            return "CSM";
    }

    return "CSM";
}

bool drawBackgroundModeCombo(luna::SceneBackgroundMode& mode)
{
    return luna::editor::ui::drawCombo("Background", backgroundModeLabel(mode), [&]() {
        bool changed = false;
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
        return changed;
    });
}

bool drawShadowModeCombo(luna::SceneShadowMode& mode)
{
    return luna::editor::ui::drawCombo("Mode", shadowModeLabel(mode), [&]() {
        bool changed = false;
        const luna::SceneShadowMode modes[] = {
            luna::SceneShadowMode::CascadedShadowMaps,
            luna::SceneShadowMode::PcfShadowMap,
            luna::SceneShadowMode::None,
        };

        for (const luna::SceneShadowMode candidate : modes) {
            const bool selected = mode == candidate;
            if (ImGui::Selectable(shadowModeLabel(candidate), selected)) {
                mode = candidate;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        return changed;
    });
}

uint32_t sanitizeShadowMapSize(int size, uint32_t fallback)
{
    constexpr int kMinShadowMapSize = 256;
    constexpr int kMaxShadowMapSize = 8192;
    return static_cast<uint32_t>(std::clamp(size <= 0 ? static_cast<int>(fallback) : size,
                                           kMinShadowMapSize,
                                           kMaxShadowMapSize));
}

bool drawShadowMapSize(const char* label, uint32_t& size)
{
    int value = static_cast<int>(size);
    const bool changed = luna::editor::ui::drawInt(label, value, 256, 1024);
    if (changed) {
        size = sanitizeShadowMapSize(value, size);
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
    m_shadow_draft = m_editor_context->getScene().shadowSettings();
    m_environment_draft_dirty = false;
    m_shadow_draft_dirty = false;
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

    ImGui::SetNextWindowSize(editor::ui::scaled(380.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Settings");

    const SceneEnvironmentSettings& scene_environment = m_editor_context->getScene().environmentSettings();
    SceneEnvironmentSettings& environment = m_environment_draft;

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawBackgroundModeCombo(environment.backgroundMode);
        environment.enabled = environment.backgroundMode != SceneBackgroundMode::SolidColor;

        if (environment.backgroundMode == SceneBackgroundMode::SolidColor) {
            editor::ui::drawColor3("Background Color", environment.backgroundColor);
        }

        editor::ui::drawBool("IBL Enabled", environment.iblEnabled);

        if (environment.backgroundMode == SceneBackgroundMode::EnvironmentMap ||
            environment.backgroundMode == SceneBackgroundMode::SolidColor) {
            editor::ui::drawAssetHandleSelector("Environment Map", environment.environmentMapHandle, {AssetType::Texture});
        }

        editor::ui::drawFloat("Intensity", environment.intensity, 0.01f, 0.0f, 100.0f, "%.2f");
        editor::ui::drawFloat("Sky Intensity", environment.skyIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        editor::ui::drawFloat("Diffuse Intensity", environment.diffuseIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        editor::ui::drawFloat("Specular Intensity", environment.specularIntensity, 0.01f, 0.0f, 100.0f, "%.2f");

        if (environment.backgroundMode == SceneBackgroundMode::ProceduralSky) {
            ImGui::SeparatorText("Default Sky");
            editor::ui::drawVec3Control("Sun Direction",
                                         environment.proceduralSunDirection,
                                         0.0f,
                                         0.01f,
                                         editor::ui::PropertyLayout{120.0f});
            editor::ui::drawFloat("Sun Intensity",
                                  environment.proceduralSunIntensity,
                                  0.05f,
                                  0.0f,
                                  1000.0f,
                                  "%.2f");
            editor::ui::drawFloat("Sun Angular Radius",
                                  environment.proceduralSunAngularRadius,
                                  0.001f,
                                  0.0f,
                                  0.25f,
                                  "%.4f");
            editor::ui::drawColor3("Sky Zenith", environment.proceduralSkyColorZenith);
            editor::ui::drawColor3("Sky Horizon", environment.proceduralSkyColorHorizon);
            editor::ui::drawColor3("Ground", environment.proceduralGroundColor);
            editor::ui::drawFloat("Sky Exposure", environment.proceduralSkyExposure, 0.01f, 0.0f, 100.0f, "%.2f");
        }

        m_environment_draft_dirty = !sameEnvironmentSettings(environment, scene_environment);

        ImGui::Separator();
        const bool disable_apply_controls = !m_environment_draft_dirty;
        if (disable_apply_controls) {
            ImGui::BeginDisabled();
        }
        if (editor::ui::drawButton("Apply", editor::ui::ButtonVariant::Primary, editor::ui::scaled(120.0f, 0.0f))) {
            m_environment_draft.enabled = m_environment_draft.backgroundMode != SceneBackgroundMode::SolidColor;
            if (m_editor_context->setSceneEnvironmentSettings(m_environment_draft)) {
                m_environment_draft_dirty = false;
            }
        }
        ImGui::SameLine();
        if (editor::ui::drawButton("Revert", editor::ui::ButtonVariant::Subtle, editor::ui::scaled(120.0f, 0.0f))) {
            m_environment_draft = scene_environment;
            m_environment_draft.enabled = m_environment_draft.backgroundMode != SceneBackgroundMode::SolidColor;
            m_environment_draft_dirty = false;
        }
        if (disable_apply_controls) {
            ImGui::EndDisabled();
        }

        if (m_environment_draft_dirty) {
            ImGui::TextDisabled("Environment changes are pending.");
        }
    }

    if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        const SceneShadowSettings& scene_shadows = m_editor_context->getScene().shadowSettings();
        SceneShadowSettings& shadows = m_shadow_draft;

        drawShadowModeCombo(shadows.mode);
        if (shadows.mode == SceneShadowMode::PcfShadowMap) {
            editor::ui::drawFloat("Shadow Distance",
                                  shadows.pcfShadowDistance,
                                  1.0f,
                                  1.0f,
                                  1000.0f,
                                  "%.1f");
            drawShadowMapSize("Resolution", shadows.pcfMapSize);
        } else if (shadows.mode == SceneShadowMode::CascadedShadowMaps) {
            drawShadowMapSize("Cascade Size", shadows.csmCascadeSize);
        }
        m_shadow_draft_dirty = !sameShadowSettings(shadows, scene_shadows);

        ImGui::Separator();
        const bool disable_apply_controls = !m_shadow_draft_dirty;
        if (disable_apply_controls) {
            ImGui::BeginDisabled();
        }
        if (editor::ui::drawButton("Apply##Shadows",
                                   editor::ui::ButtonVariant::Primary,
                                   editor::ui::scaled(120.0f, 0.0f))) {
            m_shadow_draft.pcfShadowDistance =
                std::clamp(m_shadow_draft.pcfShadowDistance, 1.0f, 1000.0f);
            m_shadow_draft.pcfMapSize =
                sanitizeShadowMapSize(static_cast<int>(m_shadow_draft.pcfMapSize), 4096);
            m_shadow_draft.csmCascadeSize =
                sanitizeShadowMapSize(static_cast<int>(m_shadow_draft.csmCascadeSize), 2048);
            if (m_editor_context->setSceneShadowSettings(m_shadow_draft)) {
                m_shadow_draft_dirty = false;
            }
        }
        ImGui::SameLine();
        if (editor::ui::drawButton("Revert##Shadows",
                                   editor::ui::ButtonVariant::Subtle,
                                   editor::ui::scaled(120.0f, 0.0f))) {
            m_shadow_draft = scene_shadows;
            m_shadow_draft_dirty = false;
        }
        if (disable_apply_controls) {
            ImGui::EndDisabled();
        }

        if (m_shadow_draft_dirty) {
            ImGui::TextDisabled("Shadow changes are pending.");
        }
    }

    ImGui::End();
}

} // namespace luna
