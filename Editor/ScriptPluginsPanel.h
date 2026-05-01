#pragma once

namespace luna {

class LunaEditorLayer;

class ScriptPluginsPanel {
public:
    explicit ScriptPluginsPanel(LunaEditorLayer& editor_layer);

    void onImGuiRender(bool& open);

private:
    LunaEditorLayer* m_editor_layer{nullptr};
};

} // namespace luna
