#pragma once

#include "Scene/Scene.h"

namespace luna {

class EditorContext;

class SceneSettingPanel {
public:
    explicit SceneSettingPanel(EditorContext& editor_context);

    void onImGuiRender();
    void syncFromScene();

private:
    EditorContext* m_editor_context{nullptr};
    SceneEnvironmentSettings m_environment_draft{};
    SceneShadowSettings m_shadow_draft{};
    bool m_environment_draft_dirty{false};
    bool m_shadow_draft_dirty{false};
    bool m_has_environment_draft{false};
};

} // namespace luna
