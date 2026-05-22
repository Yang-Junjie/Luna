#include "Authoring/EditorSceneFileController.h"

#include "Authoring/EditorAuthoringController.h"
#include "Core/Log.h"
#include "Platform/Common/FileDialogs.h"
#include "Project/EditorProjectSessionController.h"
#include "Scene/SceneSerializer.h"

namespace {

constexpr const char* kSceneFileFilter = "Luna Scene (*.lunascene)\0*.lunascene\0";

} // namespace

namespace luna {

bool EditorSceneFileController::openSceneDialog(EditorAuthoringController& authoring,
                                                EditorProjectSessionController& project_session,
                                                bool update_project_start_scene)
{
    const std::filesystem::path scene_file_path =
        FileDialogs::openFile(kSceneFileFilter, project_session.sceneDialogDefaultPath(authoring.sceneFilePath()).string());
    if (scene_file_path.empty()) {
        return false;
    }

    return openScene(scene_file_path, authoring, project_session, update_project_start_scene);
}

bool EditorSceneFileController::openScene(const std::filesystem::path& scene_file_path,
                                          EditorAuthoringController& authoring,
                                          EditorProjectSessionController& project_session,
                                          bool update_project_start_scene)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    if (!authoring.openScene(normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to open scene '{}'", normalized_scene_path.string());
        return false;
    }

    if (update_project_start_scene) {
        project_session.syncStartScene(normalized_scene_path);
    }

    LUNA_EDITOR_INFO(
        "Opened scene '{}' with {} entities",
        normalized_scene_path.string(),
        authoring.scene().entityManager().entityCount());
    return true;
}

bool EditorSceneFileController::saveScene(EditorAuthoringController& authoring,
                                          EditorProjectSessionController& project_session)
{
    if (authoring.sceneFilePath().empty()) {
        return saveSceneAsDialog(authoring, project_session);
    }

    return saveSceneAs(authoring.sceneFilePath(), authoring, project_session);
}

bool EditorSceneFileController::saveSceneAsDialog(EditorAuthoringController& authoring,
                                                  EditorProjectSessionController& project_session)
{
    const std::filesystem::path scene_file_path =
        FileDialogs::saveFile(kSceneFileFilter, project_session.sceneDialogDefaultPath(authoring.sceneFilePath()).string());
    if (scene_file_path.empty()) {
        return false;
    }

    return saveSceneAs(scene_file_path, authoring, project_session);
}

bool EditorSceneFileController::saveSceneAs(const std::filesystem::path& scene_file_path,
                                            EditorAuthoringController& authoring,
                                            EditorProjectSessionController& project_session)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    if (!authoring.saveSceneAs(normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to save scene '{}'", normalized_scene_path.string());
        return false;
    }

    project_session.syncStartScene(normalized_scene_path);

    LUNA_EDITOR_INFO("Saved scene '{}' to '{}'", authoring.scene().getName(), normalized_scene_path.string());
    return true;
}

} // namespace luna
