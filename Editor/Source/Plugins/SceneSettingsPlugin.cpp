#include "Plugins/SceneSettingsPlugin.h"

#include "EditorApi/EditorApi.h"

#include <algorithm>
#include <array>
#include <string>

namespace {

constexpr const char* kPluginId = "luna.editor.scene-settings";
constexpr const char* kWindowId = "luna.editor.scene-settings.window";

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

luna::editor::Vec3 toEditorVec3(const glm::vec3& value)
{
    return luna::editor::Vec3{.x = value.x, .y = value.y, .z = value.z};
}

void fromEditorVec3(const luna::editor::Vec3& value, glm::vec3& out_value)
{
    out_value = glm::vec3{value.x, value.y, value.z};
}

luna::SceneEnvironmentSettings normalizeEnvironmentSettings(luna::SceneEnvironmentSettings settings)
{
    settings.enabled = settings.backgroundMode != luna::SceneBackgroundMode::SolidColor;
    return settings;
}

bool sameEnvironmentSettings(const luna::SceneEnvironmentSettings& lhs, const luna::SceneEnvironmentSettings& rhs)
{
    const luna::SceneEnvironmentSettings normalized_lhs = normalizeEnvironmentSettings(lhs);
    const luna::SceneEnvironmentSettings normalized_rhs = normalizeEnvironmentSettings(rhs);
    return normalized_lhs.backgroundMode == normalized_rhs.backgroundMode &&
           sameVec3(normalized_lhs.backgroundColor, normalized_rhs.backgroundColor) &&
           normalized_lhs.iblEnabled == normalized_rhs.iblEnabled &&
           normalized_lhs.environmentMapHandle == normalized_rhs.environmentMapHandle &&
           normalized_lhs.intensity == normalized_rhs.intensity &&
           normalized_lhs.skyIntensity == normalized_rhs.skyIntensity &&
           normalized_lhs.diffuseIntensity == normalized_rhs.diffuseIntensity &&
           normalized_lhs.specularIntensity == normalized_rhs.specularIntensity &&
           sameVec3(normalized_lhs.proceduralSunDirection, normalized_rhs.proceduralSunDirection) &&
           normalized_lhs.proceduralSunIntensity == normalized_rhs.proceduralSunIntensity &&
           normalized_lhs.proceduralSunAngularRadius == normalized_rhs.proceduralSunAngularRadius &&
           sameVec3(normalized_lhs.proceduralSkyColorZenith, normalized_rhs.proceduralSkyColorZenith) &&
           sameVec3(normalized_lhs.proceduralSkyColorHorizon, normalized_rhs.proceduralSkyColorHorizon) &&
           sameVec3(normalized_lhs.proceduralGroundColor, normalized_rhs.proceduralGroundColor) &&
           normalized_lhs.proceduralSkyExposure == normalized_rhs.proceduralSkyExposure;
}

bool sameShadowSettings(const luna::SceneShadowSettings& lhs, const luna::SceneShadowSettings& rhs)
{
    return lhs.mode == rhs.mode && lhs.pcfShadowDistance == rhs.pcfShadowDistance &&
           lhs.pcfMapSize == rhs.pcfMapSize && lhs.csmCascadeSize == rhs.csmCascadeSize;
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

bool drawBackgroundModeCombo(luna::editor::Ui& ui, luna::SceneBackgroundMode& mode)
{
    const bool opened = ui.beginCombo("Background", backgroundModeLabel(mode));
    if (!opened) {
        return false;
    }

    bool changed = false;
    const std::array modes{
        luna::SceneBackgroundMode::SolidColor,
        luna::SceneBackgroundMode::ProceduralSky,
        luna::SceneBackgroundMode::EnvironmentMap,
    };
    for (const luna::SceneBackgroundMode candidate : modes) {
        const bool selected = mode == candidate;
        if (ui.selectable(backgroundModeLabel(candidate), selected)) {
            mode = candidate;
            changed = true;
        }
        if (selected) {
            ui.setItemDefaultFocus();
        }
    }
    ui.endCombo();
    return changed;
}

bool drawShadowModeCombo(luna::editor::Ui& ui, luna::SceneShadowMode& mode)
{
    const bool opened = ui.beginCombo("Mode", shadowModeLabel(mode));
    if (!opened) {
        return false;
    }

    bool changed = false;
    const std::array modes{
        luna::SceneShadowMode::CascadedShadowMaps,
        luna::SceneShadowMode::PcfShadowMap,
        luna::SceneShadowMode::None,
    };
    for (const luna::SceneShadowMode candidate : modes) {
        const bool selected = mode == candidate;
        if (ui.selectable(shadowModeLabel(candidate), selected)) {
            mode = candidate;
            changed = true;
        }
        if (selected) {
            ui.setItemDefaultFocus();
        }
    }
    ui.endCombo();
    return changed;
}

uint32_t sanitizeShadowMapSize(int size, uint32_t fallback)
{
    constexpr int kMinShadowMapSize = 256;
    constexpr int kMaxShadowMapSize = 8192;
    return static_cast<uint32_t>(std::clamp(size <= 0 ? static_cast<int>(fallback) : size,
                                           kMinShadowMapSize,
                                           kMaxShadowMapSize));
}

std::string environmentMapButtonLabel(luna::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "No Environment Map##EnvironmentMapDropTarget";
    }

    return "Environment Map: " + handle.toString() + "##EnvironmentMapDropTarget";
}

bool drawEnvironmentMapSelector(luna::editor::Ui& ui, luna::AssetHandle& handle)
{
    bool changed = false;

    ui.text("Environment Map");
    const bool button_clicked =
        ui.button(environmentMapButtonLabel(handle), luna::editor::Vec2{.x = -1.0f, .y = ui.scale(42.0f)});
    (void) button_clicked;

    const bool hovered = ui.isItemHovered();
    if (ui.beginDragDropTarget()) {
        luna::editor::AssetDropPayload payload{};
        if (ui.acceptAssetDragDropPayload(payload, {luna::AssetType::Texture}) && handle != payload.handle) {
            handle = payload.handle;
            changed = true;
        }
        ui.endDragDropTarget();
    }

    if (ui.beginPopupContextItem("EnvironmentMapContext")) {
        if (ui.menuItem("Clear", false, handle.isValid())) {
            handle = luna::AssetHandle(0);
            changed = true;
        }
        ui.endPopup();
    }

    if (hovered) {
        ui.setTooltip("Drop a texture asset here. Right-click to clear.");
    }

    return changed;
}

} // namespace

namespace luna::editor {

class SceneSettingsPlugin final : public Plugin {
public:
    [[nodiscard]] PluginDescriptor descriptor() const override
    {
        return PluginDescriptor{
            .id = kPluginId,
            .display_name = "Scene Settings",
            .version = "0.1.0",
        };
    }

    bool onLoad(Host& host) override
    {
        return host.windows().registerWindow(WindowDescriptor{
            .id = kWindowId,
            .title = "Scene Settings",
            .default_open = true,
            .default_size = Vec2{.x = 380.0f, .y = 520.0f},
            .draw =
                [this](WindowDrawContext& context) {
                    drawSceneSettingsWindow(context);
                },
        });
    }

    void onUnload(Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }

private:
    void syncEnvironmentDraft(const SceneEnvironmentSettings& scene_environment)
    {
        m_environment_draft = normalizeEnvironmentSettings(scene_environment);
        m_environment_draft_dirty = false;
        m_has_environment_draft = true;
    }

    void syncShadowDraft(const SceneShadowSettings& scene_shadows)
    {
        m_shadow_draft = scene_shadows;
        m_shadow_draft_dirty = false;
        m_has_shadow_draft = true;
    }

    void drawSceneSettingsWindow(WindowDrawContext& context)
    {
        Host& host = context.host();
        Ui& ui = context.ui();

        const SceneEnvironmentSettings current_environment = host.scene().sceneEnvironmentSettings();
        const SceneShadowSettings current_shadows = host.scene().sceneShadowSettings();

        if (!m_has_environment_draft ||
            (!m_environment_draft_dirty && !sameEnvironmentSettings(m_environment_draft, current_environment))) {
            syncEnvironmentDraft(current_environment);
        }
        if (!m_has_shadow_draft || (!m_shadow_draft_dirty && !sameShadowSettings(m_shadow_draft, current_shadows))) {
            syncShadowDraft(current_shadows);
        }

        if (!host.scene().canEditScene()) {
            ui.textDisabled("Runtime viewport is active; scene editing is disabled.");
        }

        ui.text("Environment");
        ui.separator();

        if (!host.scene().canEditScene()) {
            ui.beginDisabled();
        }
        if (drawBackgroundModeCombo(ui, m_environment_draft.backgroundMode)) {
            m_environment_draft.enabled = m_environment_draft.backgroundMode != SceneBackgroundMode::SolidColor;
        }
        m_environment_draft.enabled = m_environment_draft.backgroundMode != SceneBackgroundMode::SolidColor;

        if (m_environment_draft.backgroundMode == SceneBackgroundMode::SolidColor) {
            luna::editor::Vec3 background_color = toEditorVec3(m_environment_draft.backgroundColor);
            if (ui.colorEdit3("Background Color", background_color)) {
                fromEditorVec3(background_color, m_environment_draft.backgroundColor);
            }
        }

        ui.checkbox("IBL Enabled", m_environment_draft.iblEnabled);

        if (m_environment_draft.backgroundMode == SceneBackgroundMode::EnvironmentMap ||
            m_environment_draft.backgroundMode == SceneBackgroundMode::SolidColor) {
            (void) drawEnvironmentMapSelector(ui, m_environment_draft.environmentMapHandle);
        }

        ui.dragFloat("Intensity", m_environment_draft.intensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ui.dragFloat("Sky Intensity", m_environment_draft.skyIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ui.dragFloat("Diffuse Intensity", m_environment_draft.diffuseIntensity, 0.01f, 0.0f, 100.0f, "%.2f");
        ui.dragFloat("Specular Intensity", m_environment_draft.specularIntensity, 0.01f, 0.0f, 100.0f, "%.2f");

        if (m_environment_draft.backgroundMode == SceneBackgroundMode::ProceduralSky) {
            ui.separator();
            ui.text("Default Sky");
            luna::editor::Vec3 sun_direction = toEditorVec3(m_environment_draft.proceduralSunDirection);
            if (ui.dragFloat3("Sun Direction", sun_direction, 0.01f, 0.0f, 0.0f, "%.2f")) {
                fromEditorVec3(sun_direction, m_environment_draft.proceduralSunDirection);
            }
            ui.dragFloat("Sun Intensity",
                         m_environment_draft.proceduralSunIntensity,
                         0.05f,
                         0.0f,
                         1000.0f,
                         "%.2f");
            ui.dragFloat("Sun Angular Radius",
                         m_environment_draft.proceduralSunAngularRadius,
                         0.001f,
                         0.0f,
                         0.25f,
                         "%.4f");
            luna::editor::Vec3 sky_zenith = toEditorVec3(m_environment_draft.proceduralSkyColorZenith);
            if (ui.colorEdit3("Sky Zenith", sky_zenith)) {
                fromEditorVec3(sky_zenith, m_environment_draft.proceduralSkyColorZenith);
            }
            luna::editor::Vec3 sky_horizon = toEditorVec3(m_environment_draft.proceduralSkyColorHorizon);
            if (ui.colorEdit3("Sky Horizon", sky_horizon)) {
                fromEditorVec3(sky_horizon, m_environment_draft.proceduralSkyColorHorizon);
            }
            luna::editor::Vec3 ground_color = toEditorVec3(m_environment_draft.proceduralGroundColor);
            if (ui.colorEdit3("Ground", ground_color)) {
                fromEditorVec3(ground_color, m_environment_draft.proceduralGroundColor);
            }
            ui.dragFloat("Sky Exposure",
                         m_environment_draft.proceduralSkyExposure,
                         0.01f,
                         0.0f,
                         100.0f,
                         "%.2f");
        }

        if (!host.scene().canEditScene()) {
            ui.endDisabled();
        }

        m_environment_draft_dirty = !sameEnvironmentSettings(m_environment_draft, current_environment);

        ui.separator();
        const bool disable_environment_actions = !m_environment_draft_dirty || !host.scene().canEditScene();
        if (disable_environment_actions) {
            ui.beginDisabled();
        }
        if (ui.button("Apply", Vec2{.x = 120.0f, .y = 0.0f})) {
            m_environment_draft = normalizeEnvironmentSettings(m_environment_draft);
            if (host.scene().setSceneEnvironmentSettings(m_environment_draft)) {
                syncEnvironmentDraft(host.scene().sceneEnvironmentSettings());
            }
        }
        ui.sameLine();
        if (ui.button("Revert", Vec2{.x = 120.0f, .y = 0.0f})) {
            syncEnvironmentDraft(current_environment);
        }
        if (disable_environment_actions) {
            ui.endDisabled();
        }
        if (m_environment_draft_dirty) {
            ui.textDisabled("Environment changes are pending.");
        }

        ui.spacing();
        ui.text("Shadows");
        ui.separator();

        if (!host.scene().canEditScene()) {
            ui.beginDisabled();
        }
        drawShadowModeCombo(ui, m_shadow_draft.mode);
        if (m_shadow_draft.mode == SceneShadowMode::PcfShadowMap) {
            ui.dragFloat("Shadow Distance",
                         m_shadow_draft.pcfShadowDistance,
                         1.0f,
                         1.0f,
                         1000.0f,
                         "%.1f");
            int map_size = static_cast<int>(m_shadow_draft.pcfMapSize);
            if (ui.dragInt("Resolution", map_size, 1.0f, 256, 1024)) {
                m_shadow_draft.pcfMapSize = sanitizeShadowMapSize(map_size, m_shadow_draft.pcfMapSize);
            }
        } else if (m_shadow_draft.mode == SceneShadowMode::CascadedShadowMaps) {
            int cascade_size = static_cast<int>(m_shadow_draft.csmCascadeSize);
            if (ui.dragInt("Cascade Size", cascade_size, 1.0f, 256, 1024)) {
                m_shadow_draft.csmCascadeSize = sanitizeShadowMapSize(cascade_size, m_shadow_draft.csmCascadeSize);
            }
        }
        if (!host.scene().canEditScene()) {
            ui.endDisabled();
        }

        m_shadow_draft_dirty = !sameShadowSettings(m_shadow_draft, current_shadows);

        ui.separator();
        const bool disable_shadow_actions = !m_shadow_draft_dirty || !host.scene().canEditScene();
        if (disable_shadow_actions) {
            ui.beginDisabled();
        }
        if (ui.button("Apply##Shadows", Vec2{.x = 120.0f, .y = 0.0f})) {
            m_shadow_draft.pcfShadowDistance = std::clamp(m_shadow_draft.pcfShadowDistance, 1.0f, 1000.0f);
            m_shadow_draft.pcfMapSize = sanitizeShadowMapSize(static_cast<int>(m_shadow_draft.pcfMapSize), 4096);
            m_shadow_draft.csmCascadeSize = sanitizeShadowMapSize(static_cast<int>(m_shadow_draft.csmCascadeSize), 2048);
            if (host.scene().setSceneShadowSettings(m_shadow_draft)) {
                syncShadowDraft(host.scene().sceneShadowSettings());
            }
        }
        ui.sameLine();
        if (ui.button("Revert##Shadows", Vec2{.x = 120.0f, .y = 0.0f})) {
            syncShadowDraft(current_shadows);
        }
        if (disable_shadow_actions) {
            ui.endDisabled();
        }
        if (m_shadow_draft_dirty) {
            ui.textDisabled("Shadow changes are pending.");
        }
    }

private:
    SceneEnvironmentSettings m_environment_draft{};
    SceneShadowSettings m_shadow_draft{};
    bool m_environment_draft_dirty{false};
    bool m_shadow_draft_dirty{false};
    bool m_has_environment_draft{false};
    bool m_has_shadow_draft{false};
};

std::unique_ptr<Plugin> createSceneSettingsPlugin()
{
    return std::make_unique<SceneSettingsPlugin>();
}

} // namespace luna::editor
