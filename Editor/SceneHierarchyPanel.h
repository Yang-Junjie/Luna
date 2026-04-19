#pragma once

#include "Scene/Entity.h"

namespace luna {

class LunaEditorApplication;

class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(LunaEditorApplication& application);

    void onImGuiRender();

private:
    LunaEditorApplication* m_application{nullptr};
};

} // namespace luna
