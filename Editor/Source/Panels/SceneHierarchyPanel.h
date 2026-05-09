#pragma once

#include "Scene/Entity.h"

namespace luna {

class EditorContext;

class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(EditorContext& editor_context);

    void onImGuiRender();

private:
    EditorContext* m_editor_context{nullptr};
};

} // namespace luna
