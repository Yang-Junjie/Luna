#include "Platform/Common/FileDialogs.h"
#include "Project/EditorProjectSessionController.h"
#include "Shell/EditorShell.h"
#include "UI/EditorMainMenuController.h"

#include <imgui.h>
#include <string_view>

namespace {

constexpr const char* kProjectFileFilter = "Luna Project (*.lunaproj)\0*.lunaproj\0";

} // namespace

namespace luna {

EditorMainMenuController::EditorMainMenuController(Actions actions)
    : m_actions(std::move(actions))
{}

void EditorMainMenuController::dispatchShortcuts(editor::EditorShell* editor_shell) const
{
    if (editor_shell != nullptr) {
        (void) editor_shell->dispatchShortcuts();
    }
}

void EditorMainMenuController::drawDockSpace(EditorProjectSessionController& project_session,
                                             editor::EditorShell* editor_shell) const
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                    ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::Begin("##EditorDockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    const ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(ImGui::GetID("EditorDockSpace"), ImVec2(0.0f, 0.0f), dockspace_flags);

    if (ImGui::BeginMainMenuBar()) {
        drawMenuBar(project_session, editor_shell);
        ImGui::EndMainMenuBar();
    }

    ImGui::End();
}

void EditorMainMenuController::drawMenuBar(EditorProjectSessionController& project_session,
                                           editor::EditorShell* editor_shell) const
{
    const bool project_loaded = m_actions.has_project_loaded ? m_actions.has_project_loaded() : false;

    drawProjectMenu(project_session, editor_shell, project_loaded);
    drawSceneMenu(editor_shell, project_loaded);

    if (ImGui::BeginMenu("Edit")) {
        if (editor_shell != nullptr) {
            editor_shell->drawMenuItems("Edit");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Viewport")) {
        if (editor_shell != nullptr) {
            editor_shell->drawMenuItems("Viewport");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        if (editor_shell != nullptr) {
            ImGui::Separator();
            editor_shell->drawWindowMenuItems();
            editor_shell->drawMenuItems("Window");
        }
        ImGui::EndMenu();
    }

    if (editor_shell != nullptr) {
        editor_shell->drawMenuBarItems({"Project", "Scene", "Edit", "Viewport", "Window"});
    }
}

void EditorMainMenuController::drawProjectMenu(EditorProjectSessionController& project_session,
                                               editor::EditorShell* editor_shell,
                                               bool project_loaded) const
{
    if (!ImGui::BeginMenu("Project")) {
        return;
    }

    if (ImGui::MenuItem("Open Project")) {
        const std::filesystem::path project_file_path =
            FileDialogs::openFile(kProjectFileFilter, project_session.projectDialogDefaultPath().string());
        if (!project_file_path.empty() && m_actions.open_project) {
            m_actions.open_project(project_file_path);
        }
    }

    if (ImGui::MenuItem("Create New Project")) {
        const std::filesystem::path project_root_path =
            FileDialogs::selectDirectory(project_session.projectDialogDefaultPath().string());
        if (!project_root_path.empty()) {
            ProjectInfo project_info{.Name = "New Project",
                                     .Version = "0.1.0",
                                     .Author = "Junjie Yang",
                                     .Description = "A simple Luna project.",
                                     .StartScene = "./Assets/Scenes/Main.lunascene",
                                     .AssetsPath = "./Assets/"};

            if (project_session.createProject(project_root_path, project_info) && m_actions.open_project) {
                m_actions.open_project(project_root_path / (project_info.Name + ".lunaproj"));
            }
        }
    }

    if (ImGui::MenuItem("Sync Assets", nullptr, false, project_loaded) && m_actions.sync_project_assets) {
        m_actions.sync_project_assets();
    }
    if (ImGui::MenuItem("Refresh Script Plugins", nullptr, false, project_loaded) &&
        m_actions.refresh_project_script_plugins) {
        m_actions.refresh_project_script_plugins();
    }
    if (editor_shell != nullptr) {
        editor_shell->drawMenuItems("Project");
    }
    ImGui::EndMenu();
}

void EditorMainMenuController::drawSceneMenu(editor::EditorShell* editor_shell, bool project_loaded) const
{
    if (!ImGui::BeginMenu("Scene", project_loaded)) {
        return;
    }

    if (ImGui::MenuItem("Create Scene") && m_actions.create_scene) {
        m_actions.create_scene();
    }

    if (ImGui::MenuItem("Open Scene") && m_actions.open_scene) {
        m_actions.open_scene();
    }

    if (ImGui::MenuItem("Save Scene") && m_actions.save_scene) {
        m_actions.save_scene();
    }
    if (editor_shell != nullptr) {
        editor_shell->drawMenuItems("Scene");
    }
    ImGui::EndMenu();
}

} // namespace luna
