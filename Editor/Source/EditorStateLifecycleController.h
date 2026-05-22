#pragma once

namespace luna {

class EditorAuthoringController;
class EditorRuntimeViewportController;
class EditorViewportCoordinator;
class Renderer;
class SceneViewportInstance;

class EditorStateLifecycleController final {
public:
    void resetRuntimeViewportState(EditorRuntimeViewportController& runtime_viewport,
                                   EditorViewportCoordinator& viewports) const;
    void resetEditorDocumentState(EditorRuntimeViewportController& runtime_viewport,
                                  EditorViewportCoordinator& viewports,
                                  EditorAuthoringController& authoring,
                                  bool& pick_debug_visualization_enabled,
                                  SceneViewportInstance& active_viewport,
                                  Renderer* renderer,
                                  bool editor_grid_enabled) const;

    void resetAuthoringViewportState(EditorRuntimeViewportController& runtime_viewport,
                                     EditorViewportCoordinator& viewports,
                                     bool& pick_debug_visualization_enabled,
                                     SceneViewportInstance& active_viewport,
                                     Renderer* renderer,
                                     bool editor_grid_enabled) const;

    void syncPickDebugVisualization(const SceneViewportInstance& active_viewport,
                                    Renderer* renderer,
                                    bool enabled) const;
    void syncEditorGrid(const SceneViewportInstance& active_viewport,
                        Renderer* renderer,
                        bool enabled,
                        const EditorRuntimeViewportController& runtime_viewport) const;
};

} // namespace luna
