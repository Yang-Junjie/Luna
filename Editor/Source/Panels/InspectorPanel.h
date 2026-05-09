#pragma once

namespace luna {

class EditorContext;

class InspectorPanel {
public:
    explicit InspectorPanel(EditorContext& editor_context);

    void onImGuiRender();

private:
    EditorContext* m_editor_context{nullptr};
};

} // namespace luna
