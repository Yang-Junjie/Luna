#pragma once

namespace luna::editor {

class EditorPanel {
public:
    virtual ~EditorPanel() = default;

    virtual void onAttach() {}

    virtual void onDetach() {}

    virtual void onImGuiRender() = 0;
};

} // namespace luna::editor
