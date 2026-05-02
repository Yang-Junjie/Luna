#pragma once

namespace luna {

class LunaEditorLayer;

class InspectorPanel {
public:
    explicit InspectorPanel(LunaEditorLayer& editor_layer);

    void onImGuiRender();

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

} // namespace luna
