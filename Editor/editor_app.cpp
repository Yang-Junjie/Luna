#include "Editor/editor_app.h"

#include "Editor/editor_layer.h"
#include "Renderer/SceneRenderPipeline.h"

namespace luna::editor {

EditorApp::EditorApp()
    : Application(ApplicationSpecification{
          .name = "Luna Editor",
          .windowWidth = 1'700,
          .windowHeight = 900,
          .maximized = false,
          .renderService =
              {
                  .applicationName = "Luna Editor",
                  .backend = luna::RHIBackend::Vulkan,
                  .renderPipeline = luna::CreateDefaultSceneRenderPipeline(),
              },
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
