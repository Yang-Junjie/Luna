#include "Core/Application.h"
#include "Core/Log.h"
#include "Editor/EditorRegistry.h"
#include "EditorCameraControllerLayer.h"
#include "Plugin/PluginRegistry.h"
#include "RendererInfoPanel.h"

#include <glm/vec3.hpp>
#include <memory>

namespace {

void resetMainCamera()
{
    auto& camera = luna::Application::get().getRenderer().getMainCamera();
    camera.m_position = glm::vec3(0.0f, 0.0f, 5.0f);
    camera.m_pitch = 0.0f;
    camera.m_yaw = 0.0f;
    camera.m_velocity = glm::vec3(0.0f);
}

} // namespace

extern "C" void luna_register_luna_editor_core(luna::PluginRegistry& registry)
{
    registry.addLayer("luna.editor.camera_controller", [] {
        return std::make_unique<luna::editor::EditorCameraControllerLayer>();
    });

    if (!registry.hasEditorRegistry()) {
        LUNA_EDITOR_WARN("Editor registry is unavailable, skipping editor panel contributions");
        return;
    }

    auto& editor_registry = registry.editor();
    editor_registry.addPanel<luna::editor::RendererInfoPanel>("luna.editor.renderer", "Renderer", true);
    editor_registry.addCommand("luna.editor.reset_camera", "Reset Camera", [] {
        resetMainCamera();
    });
}
