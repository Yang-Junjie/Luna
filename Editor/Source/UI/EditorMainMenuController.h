#pragma once

#include "Project/ProjectInfo.h"

#include <filesystem>
#include <functional>

namespace luna {

class EditorProjectSessionController;

namespace editor {
class EditorShell;
}

class EditorMainMenuController final {
public:
    struct Actions {
        std::function<bool()> has_project_loaded;
        std::function<void(const std::filesystem::path&)> open_project;
        std::function<void()> sync_project_assets;
        std::function<void()> refresh_project_script_plugins;
        std::function<void()> create_scene;
        std::function<void()> open_scene;
        std::function<void()> save_scene;
    };

    explicit EditorMainMenuController(Actions actions);

    void dispatchShortcuts(editor::EditorShell* editor_shell) const;
    void drawDockSpace(EditorProjectSessionController& project_session, editor::EditorShell* editor_shell) const;

private:
    void drawMenuBar(EditorProjectSessionController& project_session, editor::EditorShell* editor_shell) const;
    void drawProjectMenu(EditorProjectSessionController& project_session,
                         editor::EditorShell* editor_shell,
                         bool project_loaded) const;
    void drawSceneMenu(editor::EditorShell* editor_shell, bool project_loaded) const;

    Actions m_actions;
};

} // namespace luna
