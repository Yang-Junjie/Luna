#pragma once

#include "Scene/Scene.h"

namespace luna {

class LunaEditorLayer;

class SceneSettingPanel {
public:
    explicit SceneSettingPanel(LunaEditorLayer& editor_layer);

    void onImGuiRender();
    void syncFromScene();

private:
    LunaEditorLayer* m_editor_layer{nullptr};
    SceneEnvironmentSettings m_environment_draft{};
    bool m_environment_draft_dirty{false};
    bool m_has_environment_draft{false};
};

} // namespace luna
