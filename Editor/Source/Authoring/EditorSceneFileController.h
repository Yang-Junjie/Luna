#pragma once

#include <filesystem>

namespace luna {

class EditorAuthoringController;
class EditorProjectSessionController;

class EditorSceneFileController final {
public:
    [[nodiscard]] bool openSceneDialog(EditorAuthoringController& authoring,
                                       EditorProjectSessionController& project_session,
                                       bool update_project_start_scene);
    [[nodiscard]] bool openScene(const std::filesystem::path& scene_file_path,
                                 EditorAuthoringController& authoring,
                                 EditorProjectSessionController& project_session,
                                 bool update_project_start_scene);

    [[nodiscard]] bool saveScene(EditorAuthoringController& authoring,
                                 EditorProjectSessionController& project_session);
    [[nodiscard]] bool saveSceneAsDialog(EditorAuthoringController& authoring,
                                         EditorProjectSessionController& project_session);
    [[nodiscard]] bool saveSceneAs(const std::filesystem::path& scene_file_path,
                                   EditorAuthoringController& authoring,
                                   EditorProjectSessionController& project_session);
};

} // namespace luna
