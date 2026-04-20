#pragma once

#include "Scene/Entity.h"

namespace luna {

class LunaEditorLayer;

class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(LunaEditorLayer& editor_layer);

    void onImGuiRender();

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

} // namespace luna
