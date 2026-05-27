#pragma once

#include "Core/UUID.h"
#include "Scene/Entity.h"

#include <cstdint>

#include <string>

struct ImVec2;

namespace luna {

class EditorAuthoringController;
class EditorCamera;

enum class GizmoOperation : uint8_t {
    Translate,
    Rotate,
    Scale,
};

enum class GizmoMode : uint8_t {
    Local,
    World,
};

class EditorViewportGizmoController final {
public:
    [[nodiscard]] bool hasActiveTransformTransaction() const noexcept;
    void commitActiveTransformTransaction(EditorAuthoringController& authoring);

    void updateShortcuts(bool viewport_focused,
                         bool editor_camera_mouse_captured,
                         bool text_input_active,
                         UUID selected_entity_id);
    [[nodiscard]] bool draw(const ImVec2& viewport_min,
                            const ImVec2& viewport_size,
                            bool allow_interaction,
                            const EditorCamera& editor_camera,
                            EditorAuthoringController& authoring,
                            Entity selected_entity);

    [[nodiscard]] std::string operationName() const;
    [[nodiscard]] std::string modeName() const;

private:
    GizmoOperation m_operation{GizmoOperation::Translate};
    GizmoMode m_mode{GizmoMode::Local};
    bool m_transform_transaction_active{false};
};

} // namespace luna
