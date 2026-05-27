#include "Authoring/EditorAuthoringController.h"
#include "EditorRuntimeViewportController.h"
#include "EditorStateLifecycleController.h"
#include "Viewport/EditorViewportCoordinator.h"
#include "Viewport/SceneViewportInstance.h"

namespace luna {

void EditorStateLifecycleController::resetRuntimeViewportState(EditorRuntimeViewportController& runtime_viewport,
                                                               EditorViewportCoordinator& viewports) const
{
    runtime_viewport.resetRuntimeViewport();
    viewports.clearViewportInteraction(EditorViewportCoordinator::runtimeSceneViewportId());
}

void EditorStateLifecycleController::resetEditorDocumentState(EditorRuntimeViewportController& runtime_viewport,
                                                              EditorViewportCoordinator& viewports,
                                                              EditorAuthoringController& authoring,
                                                              bool& pick_debug_visualization_enabled,
                                                              SceneViewportInstance& active_viewport,
                                                              Renderer* renderer,
                                                              bool editor_grid_enabled) const
{
    resetRuntimeViewportState(runtime_viewport, viewports);
    authoring.reset();
    pick_debug_visualization_enabled = false;
    syncPickDebugVisualization(active_viewport, renderer, pick_debug_visualization_enabled);
    syncEditorGrid(active_viewport, renderer, editor_grid_enabled, runtime_viewport);
}

void EditorStateLifecycleController::resetAuthoringViewportState(EditorRuntimeViewportController& runtime_viewport,
                                                                 EditorViewportCoordinator& viewports,
                                                                 bool& pick_debug_visualization_enabled,
                                                                 SceneViewportInstance& active_viewport,
                                                                 Renderer* renderer,
                                                                 bool editor_grid_enabled) const
{
    resetRuntimeViewportState(runtime_viewport, viewports);
    pick_debug_visualization_enabled = false;
    syncPickDebugVisualization(active_viewport, renderer, pick_debug_visualization_enabled);
    syncEditorGrid(active_viewport, renderer, editor_grid_enabled, runtime_viewport);
}

void EditorStateLifecycleController::syncPickDebugVisualization(const SceneViewportInstance& active_viewport,
                                                                Renderer* renderer,
                                                                bool enabled) const
{
    if (renderer == nullptr) {
        return;
    }

    active_viewport.setPickDebugVisualization(*renderer, enabled);
}

void EditorStateLifecycleController::syncEditorGrid(const SceneViewportInstance& active_viewport,
                                                    Renderer* renderer,
                                                    bool enabled,
                                                    const EditorRuntimeViewportController& runtime_viewport) const
{
    if (renderer == nullptr) {
        return;
    }

    active_viewport.setEditorGrid(*renderer, enabled, runtime_viewport.isRuntimeViewportEnabled());
}

} // namespace luna
