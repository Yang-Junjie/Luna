#include "Authoring/EditorAuthoringController.h"
#include "EditorCamera.h"
#include "EditorRuntimeSessionController.h"
#include "EditorRuntimeViewportController.h"
#include "EditorStateLifecycleController.h"
#include "Project/EditorProjectSessionController.h"
#include "Script/ScriptPluginManager.h"
#include "Script/ScriptRuntime.h"
#include "Viewport/EditorViewportCoordinator.h"
#include "Viewport/EditorViewportGizmoController.h"

#include <memory>

namespace luna {

bool EditorRuntimeSessionController::setRuntimeViewportEnabled(bool enabled,
                                                               EditorRuntimeViewportController& runtime_viewport,
                                                               EditorAuthoringController& authoring,
                                                               EditorProjectSessionController& project_session,
                                                               EditorViewportCoordinator& viewports,
                                                               EditorViewportGizmoController& gizmo,
                                                               EditorStateLifecycleController& lifecycle,
                                                               EditorCamera& editor_camera,
                                                               Renderer* renderer,
                                                               bool editor_grid_enabled) const
{
    if (enabled == runtime_viewport.isRuntimeViewportEnabled()) {
        return true;
    }

    gizmo.commitActiveTransformTransaction(authoring);

    std::unique_ptr<IScriptRuntime> script_runtime;
    if (enabled) {
        script_runtime = ScriptPluginManager::instance().createRuntimeForProject(project_session.projectInfo());
    }

    const bool changed =
        runtime_viewport.setRuntimeViewportEnabled(enabled, authoring.scene(), std::move(script_runtime));
    if (!enabled) {
        lifecycle.resetRuntimeViewportState(runtime_viewport, viewports);
    }
    if (changed && runtime_viewport.isRuntimeViewportEnabled()) {
        editor_camera.releaseMouseCapture();
        editor_camera.setInputEnabled(false);
    }

    lifecycle.syncEditorGrid(viewports.activeSceneViewport(runtime_viewport.isRuntimeViewportEnabled()),
                             renderer,
                             editor_grid_enabled,
                             runtime_viewport);
    return changed;
}

} // namespace luna
