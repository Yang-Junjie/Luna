#pragma once

#include <memory>

namespace luna {

class EditorAuthoringController;
class EditorCamera;
class EditorProjectSessionController;
class EditorRuntimeViewportController;
class EditorStateLifecycleController;
class EditorViewportCoordinator;
class EditorViewportGizmoController;
class Renderer;
class SceneViewportInstance;

class EditorRuntimeSessionController final {
public:
    [[nodiscard]] bool setRuntimeViewportEnabled(bool enabled,
                                                 EditorRuntimeViewportController& runtime_viewport,
                                                 EditorAuthoringController& authoring,
                                                 EditorProjectSessionController& project_session,
                                                 EditorViewportCoordinator& viewports,
                                                 EditorViewportGizmoController& gizmo,
                                                 EditorStateLifecycleController& lifecycle,
                                                 EditorCamera& editor_camera,
                                                 Renderer* renderer,
                                                 bool editor_grid_enabled) const;
};

} // namespace luna
