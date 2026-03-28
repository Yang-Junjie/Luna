#include "Editor/editor_app.h"

#include "Editor/editor_layer.h"

namespace luna::editor {

EditorApp::EditorApp()
    : Application(ApplicationSpecification{
          .name = "Luna Editor",
          .windowWidth = 1'700,
          .windowHeight = 900,
          .maximized = false,
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
