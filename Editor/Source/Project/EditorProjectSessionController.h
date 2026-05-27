#pragma once

#include "Project/ProjectInfo.h"

#include <filesystem>
#include <optional>

namespace luna {

namespace editor {
class EditorShell;
}

class EditorProjectSessionController final {
public:
    [[nodiscard]] static std::filesystem::path projectDialogDefaultPath();
    [[nodiscard]] static std::optional<std::filesystem::path>
        makeScenePathRelativeToProject(const std::filesystem::path& scene_file_path);

    [[nodiscard]] bool hasProjectLoaded() const;
    [[nodiscard]] std::filesystem::path projectRootPath() const;
    [[nodiscard]] const ProjectInfo* projectInfo() const;

    [[nodiscard]] bool createProject(const std::filesystem::path& project_root_path,
                                     const ProjectInfo& project_info) const;
    [[nodiscard]] bool loadProject(const std::filesystem::path& project_file_path) const;
    [[nodiscard]] bool refreshAssets(editor::EditorShell* editor_shell) const;
    void reloadProjectAssets(editor::EditorShell* editor_shell) const;
    void refreshScriptPlugins(editor::EditorShell* editor_shell) const;

    [[nodiscard]] std::optional<std::filesystem::path> configuredStartScenePath() const;
    [[nodiscard]] std::filesystem::path
        sceneDialogDefaultPath(const std::filesystem::path& current_scene_file_path) const;
    void syncStartScene(const std::filesystem::path& scene_file_path) const;
};

} // namespace luna
