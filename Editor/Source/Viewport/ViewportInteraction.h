#pragma once

#include "EditorApi/EditorTypes.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace luna {

struct ViewportSurfaceRect {
    editor::Vec2 min{};
    editor::Vec2 max{};
};

struct ViewportInteractionInput {
    ViewportSurfaceRect rect{};
    editor::Vec2 mouse_delta{};
    editor::Vec2 mouse_wheel_delta{};
    bool hovered{false};
    bool active{false};
    bool clicked{false};
    bool double_clicked{false};
    bool left_mouse_down{false};
    bool left_mouse_released{false};
    bool dragging{false};
};

struct ViewportInteractionState {
    editor::ViewportId viewport_id{editor::kInvalidViewportId};
    std::string owner_id;
    ViewportSurfaceRect rect{};
    editor::Vec2 mouse_delta{};
    editor::Vec2 mouse_drag_delta{};
    editor::Vec2 mouse_wheel_delta{};
    bool hovered{false};
    bool active{false};
    bool clicked{false};
    bool double_clicked{false};
    bool dragging{false};
    bool mouse_captured{false};
};

class ViewportInteractionTracker final {
public:
    void beginFrame() noexcept;
    void clear() noexcept;
    void clearViewport(editor::ViewportId viewport_id);
    void clearOwner(std::string_view owner_id);

    const ViewportInteractionState&
        recordSurface(editor::ViewportId viewport_id, std::string_view owner_id, const ViewportInteractionInput& input);

    void setMouseCapture(editor::ViewportId viewport_id, bool captured);

    [[nodiscard]] const ViewportInteractionState* find(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] bool isHovered(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] bool isActive(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] bool isClicked(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] bool isDoubleClicked(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] bool hasMouseCapture(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] bool allowsInput(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] editor::ViewportId hoveredViewport() const noexcept;
    [[nodiscard]] editor::ViewportId activeViewport() const noexcept;
    [[nodiscard]] editor::ViewportId capturedViewport() const noexcept;

private:
    [[nodiscard]] ViewportInteractionState& stateFor(editor::ViewportId viewport_id);

    std::unordered_map<editor::ViewportId, ViewportInteractionState> m_states;
    editor::ViewportId m_hovered_viewport{editor::kInvalidViewportId};
    editor::ViewportId m_active_viewport{editor::kInvalidViewportId};
    editor::ViewportId m_captured_viewport{editor::kInvalidViewportId};
};

} // namespace luna
