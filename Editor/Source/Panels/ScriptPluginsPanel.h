#pragma once

namespace luna {

class EditorContext;

class ScriptPluginsPanel {
public:
    explicit ScriptPluginsPanel(EditorContext& editor_context);

    void onImGuiRender(bool& open);

private:
    EditorContext* m_editor_context{nullptr};
};

} // namespace luna
