#include "Editor/EditorApp.h"

#include "Editor/EditorLayer.h"

namespace luna::editor {

EditorApp::EditorApp()
    : Application(ApplicationSpecification{
          .m_name = "Luna Editor",
          .m_window_width = 1'700,
          .m_window_height = 900,
          .m_maximized = false,
      })
{}

void EditorApp::onInit()
{
    pushLayer(std::make_unique<EditorLayer>());
}

} // namespace luna::editor

namespace luna {

Application* createApplication(int, char**)
{
    return new editor::EditorApp();
}

} // namespace luna

