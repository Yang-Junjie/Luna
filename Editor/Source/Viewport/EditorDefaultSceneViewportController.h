#pragma once

#include "EditorApi/EditorViewportService.h"
#include "Scene/Entity.h"

#include <string_view>

namespace luna {

class EditorAuthoringController;
class EditorCamera;
class EditorRuntimeViewportController;
class EditorViewportCoordinator;
class EditorViewportGizmoController;
class Renderer;
class SceneViewportInstance;

namespace editor {
class Ui;
}

class EditorDefaultSceneViewportController final {
public:
    struct DrawResult {
        bool focused{false};
    };

    [[nodiscard]] DrawResult draw(editor::Ui& ui,
                                  std::string_view owner_id,
                                  Renderer& renderer,
                                  EditorCamera& editor_camera,
                                  EditorAuthoringController& authoring,
                                  EditorRuntimeViewportController& runtime_viewport,
                                  EditorViewportCoordinator& viewports,
                                  EditorViewportGizmoController& gizmo,
                                  SceneViewportInstance& active_viewport,
                                  editor::ViewportId active_viewport_id,
                                  Entity selected_entity);
};

} // namespace luna
