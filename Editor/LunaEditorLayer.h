#pragma once

#include "Core/Layer.h"
#include "InspectorPanel.h"
#include "SceneHierarchyPanel.h"

namespace luna {

class LunaEditorApplication;

class LunaEditorLayer final : public Layer {
public:
    explicit LunaEditorLayer(LunaEditorApplication& application);

    void onAttach() override;
    void onDetach() override;
    void onImGuiRender() override;

private:
    void onImGuiMenuBar();
    void drawViewport();

private:
    LunaEditorApplication* m_application{nullptr};
    SceneHierarchyPanel m_scene_hierarchy_panel;
    InspectorPanel m_inspector_panel;
};

} // namespace luna
