#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "EditorApi/EditorAssetService.h"
#include "EditorApi/EditorScriptPluginService.h"
#include "Project/BuiltinMaterialOverrides.h"
#include "Project/EditorProjectSessionController.h"
#include "Project/ProjectManager.h"
#include "Scene/SceneSerializer.h"
#include "Shell/EditorShell.h"

#include <system_error>

namespace luna {

std::filesystem::path EditorProjectSessionController::projectDialogDefaultPath()
{
    if (const auto project_root = ProjectManager::instance().getProjectRootPath()) {
        return *project_root;
    }

    return std::filesystem::current_path();
}

std::optional<std::filesystem::path>
    EditorProjectSessionController::makeScenePathRelativeToProject(const std::filesystem::path& scene_file_path)
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    if (!project_root || scene_file_path.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(scene_file_path, *project_root, ec);
    if (ec) {
        return std::nullopt;
    }

    relative_path = relative_path.lexically_normal();
    if (relative_path.empty() || relative_path.is_absolute()) {
        return std::nullopt;
    }

    const std::string relative_string = relative_path.generic_string();
    if (relative_string == "." || relative_string.starts_with("..")) {
        return std::nullopt;
    }

    return relative_path;
}

bool EditorProjectSessionController::hasProjectLoaded() const
{
    return ProjectManager::instance().getProjectRootPath().has_value() &&
           ProjectManager::instance().getProjectInfo().has_value();
}

std::filesystem::path EditorProjectSessionController::projectRootPath() const
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    return project_root ? *project_root : std::filesystem::path{};
}

const ProjectInfo* EditorProjectSessionController::projectInfo() const
{
    const auto project_info = ProjectManager::instance().getProjectInfo();
    return project_info ? &*project_info : nullptr;
}

bool EditorProjectSessionController::createProject(const std::filesystem::path& project_root_path,
                                                   const ProjectInfo& project_info) const
{
    if (!ProjectManager::instance().createProject(project_root_path, project_info)) {
        return false;
    }

    std::error_code ec;
    if (!project_info.AssetsPath.empty()) {
        std::filesystem::create_directories((project_root_path / project_info.AssetsPath).lexically_normal(), ec);
    }

    ec.clear();
    if (!project_info.StartScene.empty()) {
        const auto scene_directory = (project_root_path / project_info.StartScene).lexically_normal().parent_path();
        if (!scene_directory.empty()) {
            std::filesystem::create_directories(scene_directory, ec);
        }
    }

    return true;
}

bool EditorProjectSessionController::loadProject(const std::filesystem::path& project_file_path) const
{
    if (project_file_path.empty()) {
        return false;
    }

    if (!ProjectManager::instance().loadProject(project_file_path)) {
        LUNA_EDITOR_WARN("Failed to load project '{}'", project_file_path.string());
        return false;
    }

    return true;
}

bool EditorProjectSessionController::refreshAssets(editor::EditorShell* editor_shell) const
{
    if (!hasProjectLoaded()) {
        LUNA_EDITOR_WARN("Cannot sync assets because no project is currently loaded");
        return false;
    }

    if (editor_shell == nullptr) {
        LUNA_EDITOR_WARN("Cannot sync assets because the editor shell is not available");
        return false;
    }

    const editor::AssetRefreshResult result = editor_shell->assets().refreshAssets();
    return result.success;
}

void EditorProjectSessionController::reloadProjectAssets(editor::EditorShell* editor_shell) const
{
    AssetManager::get().clear();
    AssetDatabase::clear();
    (void) refreshAssets(editor_shell);
    AssetManager::get().init();
    BuiltinMaterialOverrides::load();
}

void EditorProjectSessionController::refreshScriptPlugins(editor::EditorShell* editor_shell) const
{
    if (editor_shell != nullptr) {
        editor_shell->scriptPlugins().refreshProjectScriptPlugins();
    }
}

std::optional<std::filesystem::path> EditorProjectSessionController::configuredStartScenePath() const
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_root || !project_info || project_info->StartScene.empty()) {
        return std::nullopt;
    }

    return SceneSerializer::normalizeScenePath((*project_root / project_info->StartScene).lexically_normal());
}

std::filesystem::path
    EditorProjectSessionController::sceneDialogDefaultPath(const std::filesystem::path& current_scene_file_path) const
{
    if (!current_scene_file_path.empty()) {
        const std::filesystem::path parent_path = current_scene_file_path.parent_path();
        if (!parent_path.empty() && std::filesystem::exists(parent_path)) {
            return parent_path;
        }
    }

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (project_root && project_info) {
        const std::filesystem::path scenes_directory =
            (*project_root / project_info->AssetsPath / "Scenes").lexically_normal();
        if (std::filesystem::exists(scenes_directory)) {
            return scenes_directory;
        }

        const std::filesystem::path assets_directory = (*project_root / project_info->AssetsPath).lexically_normal();
        if (std::filesystem::exists(assets_directory)) {
            return assets_directory;
        }

        return *project_root;
    }

    return projectDialogDefaultPath();
}

void EditorProjectSessionController::syncStartScene(const std::filesystem::path& scene_file_path) const
{
    const auto relative_scene_path = makeScenePathRelativeToProject(scene_file_path);
    if (!relative_scene_path) {
        LUNA_EDITOR_WARN("Scene '{}' is outside the current project root. StartScene was not updated.",
                         scene_file_path.string());
        return;
    }

    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_info) {
        return;
    }

    if (project_info->StartScene.lexically_normal() == relative_scene_path->lexically_normal()) {
        return;
    }

    ProjectInfo updated_project_info = *project_info;
    updated_project_info.StartScene = *relative_scene_path;
    ProjectManager::instance().setProjectInfo(updated_project_info);

    if (ProjectManager::instance().saveProject()) {
        LUNA_EDITOR_INFO("Updated project StartScene to '{}'", relative_scene_path->generic_string());
    } else {
        LUNA_EDITOR_WARN("Failed to persist updated StartScene '{}'", relative_scene_path->generic_string());
    }
}

} // namespace luna
