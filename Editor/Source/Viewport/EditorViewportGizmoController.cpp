#include "Authoring/EditorAuthoringController.h"
#include "EditorCamera.h"
#include "Scene/Components.h"
#include "Viewport/EditorViewportGizmoController.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

namespace {

const char* gizmoOperationToString(luna::GizmoOperation operation)
{
    switch (operation) {
        case luna::GizmoOperation::Translate:
            return "Translate";
        case luna::GizmoOperation::Rotate:
            return "Rotate";
        case luna::GizmoOperation::Scale:
            return "Scale";
    }
    return "Unknown";
}

ImGuizmo::OPERATION toImGuizmoOperation(luna::GizmoOperation operation)
{
    switch (operation) {
        case luna::GizmoOperation::Translate:
            return ImGuizmo::TRANSLATE;
        case luna::GizmoOperation::Rotate:
            return ImGuizmo::ROTATE;
        case luna::GizmoOperation::Scale:
            return ImGuizmo::SCALE;
    }
    return ImGuizmo::TRANSLATE;
}

ImGuizmo::MODE toImGuizmoMode(luna::GizmoMode mode)
{
    return mode == luna::GizmoMode::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

} // namespace

namespace luna {

bool EditorViewportGizmoController::hasActiveTransformTransaction() const noexcept
{
    return m_transform_transaction_active;
}

void EditorViewportGizmoController::commitActiveTransformTransaction(EditorAuthoringController& authoring)
{
    if (!m_transform_transaction_active) {
        return;
    }

    (void) authoring.commitTransaction();
    m_transform_transaction_active = false;
}

void EditorViewportGizmoController::updateShortcuts(bool viewport_focused,
                                                    bool editor_camera_mouse_captured,
                                                    bool text_input_active,
                                                    UUID selected_entity_id)
{
    if (!viewport_focused || editor_camera_mouse_captured || text_input_active || !selected_entity_id.isValid()) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        m_operation = GizmoOperation::Translate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        m_operation = GizmoOperation::Rotate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        m_operation = GizmoOperation::Scale;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        m_mode = m_mode == GizmoMode::Local ? GizmoMode::World : GizmoMode::Local;
    }
}

bool EditorViewportGizmoController::draw(const ImVec2& viewport_min,
                                         const ImVec2& viewport_size,
                                         bool allow_interaction,
                                         const EditorCamera& editor_camera,
                                         EditorAuthoringController& authoring,
                                         Entity selected_entity)
{
    if (!selected_entity || !selected_entity.isValid() || !selected_entity.hasComponent<TransformComponent>()) {
        commitActiveTransformTransaction(authoring);
        return false;
    }

    if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
        return false;
    }

    const auto& camera = editor_camera.getCamera();
    const float aspect_ratio = viewport_size.x / viewport_size.y;
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix(aspect_ratio);
    projection[1][1] *= -1.0f;
    glm::mat4 transform = authoring.scene().entityManager().getWorldSpaceTransformMatrix(selected_entity);

    ImGuizmo::SetOrthographic(camera.getProjectionType() == Camera::ProjectionType::Orthographic);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport_min.x, viewport_min.y, viewport_size.x, viewport_size.y);
    ImGuizmo::PushID(static_cast<int>(static_cast<uint64_t>(selected_entity.getUUID()) & 0x7f'ff'ff'ff));
    ImGuizmo::Enable(allow_interaction);

    const ImGuizmo::MODE mode = m_operation == GizmoOperation::Scale ? ImGuizmo::LOCAL : toImGuizmoMode(m_mode);
    ImGuizmo::Manipulate(glm::value_ptr(view),
                         glm::value_ptr(projection),
                         toImGuizmoOperation(m_operation),
                         mode,
                         glm::value_ptr(transform));

    const bool gizmo_using = ImGuizmo::IsUsing();
    if (gizmo_using) {
        if (!m_transform_transaction_active) {
            m_transform_transaction_active = authoring.beginTransaction("Transform Entity");
        }
        authoring.scene().entityManager().setWorldSpaceTransform(selected_entity, transform);
        authoring.markSceneDirty();
    } else if (m_transform_transaction_active) {
        commitActiveTransformTransaction(authoring);
    }

    const bool gizmo_active = ImGuizmo::IsOver() || gizmo_using;
    ImGuizmo::Enable(true);
    ImGuizmo::PopID();
    return gizmo_active;
}

std::string EditorViewportGizmoController::operationName() const
{
    return gizmoOperationToString(m_operation);
}

std::string EditorViewportGizmoController::modeName() const
{
    return m_mode == GizmoMode::World ? "World" : "Local";
}

} // namespace luna
