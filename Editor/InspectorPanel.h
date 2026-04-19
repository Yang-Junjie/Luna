#pragma once

namespace luna {

class LunaEditorApplication;

class InspectorPanel {
public:
    explicit InspectorPanel(LunaEditorApplication& application);

    void onImGuiRender();

private:
    LunaEditorApplication* m_application{nullptr};
};

} // namespace luna
