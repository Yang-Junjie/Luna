#include "Viewport/ViewportInteraction.h"

namespace luna {

namespace {

std::string toOwnedString(std::string_view value)
{
    return std::string(value.data(), value.size());
}

} // namespace

void ViewportInteractionTracker::beginFrame() noexcept
{
    m_hovered_viewport = editor::kInvalidViewportId;
    m_active_viewport = editor::kInvalidViewportId;

    for (auto& [viewport_id, state] : m_states) {
        (void) viewport_id;
        state.hovered = false;
        state.active = false;
        state.clicked = false;
        state.double_clicked = false;
    }
}

void ViewportInteractionTracker::clear() noexcept
{
    m_states.clear();
    m_hovered_viewport = editor::kInvalidViewportId;
    m_active_viewport = editor::kInvalidViewportId;
    m_captured_viewport = editor::kInvalidViewportId;
}

void ViewportInteractionTracker::clearViewport(editor::ViewportId viewport_id)
{
    if (viewport_id == editor::kInvalidViewportId) {
        return;
    }

    m_states.erase(viewport_id);
    if (m_hovered_viewport == viewport_id) {
        m_hovered_viewport = editor::kInvalidViewportId;
    }
    if (m_active_viewport == viewport_id) {
        m_active_viewport = editor::kInvalidViewportId;
    }
    if (m_captured_viewport == viewport_id) {
        m_captured_viewport = editor::kInvalidViewportId;
    }
}

void ViewportInteractionTracker::clearOwner(std::string_view owner_id)
{
    if (owner_id.empty()) {
        return;
    }

    const std::string owner_key = toOwnedString(owner_id);
    for (auto it = m_states.begin(); it != m_states.end();) {
        if (it->second.owner_id == owner_key) {
            const editor::ViewportId viewport_id = it->first;
            it = m_states.erase(it);
            if (m_hovered_viewport == viewport_id) {
                m_hovered_viewport = editor::kInvalidViewportId;
            }
            if (m_active_viewport == viewport_id) {
                m_active_viewport = editor::kInvalidViewportId;
            }
            if (m_captured_viewport == viewport_id) {
                m_captured_viewport = editor::kInvalidViewportId;
            }
        } else {
            ++it;
        }
    }
}

const ViewportInteractionState& ViewportInteractionTracker::recordSurface(editor::ViewportId viewport_id,
                                                                          std::string_view owner_id,
                                                                          const ViewportInteractionInput& input)
{
    ViewportInteractionState& state = stateFor(viewport_id);
    state.owner_id = toOwnedString(owner_id);
    state.rect = input.rect;
    state.hovered = input.hovered;
    state.active = input.active;
    state.clicked = input.clicked;
    state.double_clicked = input.double_clicked;

    if (state.hovered) {
        if (m_hovered_viewport != editor::kInvalidViewportId && m_hovered_viewport != viewport_id) {
            const auto previous_hovered_it = m_states.find(m_hovered_viewport);
            if (previous_hovered_it != m_states.end()) {
                previous_hovered_it->second.hovered = false;
                previous_hovered_it->second.clicked = false;
                previous_hovered_it->second.double_clicked = false;
            }
        }
        m_hovered_viewport = viewport_id;
    }
    if (state.active) {
        if (m_active_viewport != editor::kInvalidViewportId && m_active_viewport != viewport_id) {
            const auto previous_active_it = m_states.find(m_active_viewport);
            if (previous_active_it != m_states.end()) {
                previous_active_it->second.active = false;
            }
        }
        m_active_viewport = viewport_id;
    }

    return state;
}

void ViewportInteractionTracker::setMouseCapture(editor::ViewportId viewport_id, bool captured)
{
    if (viewport_id == editor::kInvalidViewportId) {
        return;
    }

    if (captured && m_captured_viewport != editor::kInvalidViewportId && m_captured_viewport != viewport_id) {
        const auto previous_it = m_states.find(m_captured_viewport);
        if (previous_it != m_states.end()) {
            previous_it->second.mouse_captured = false;
        }
    }

    ViewportInteractionState& state = stateFor(viewport_id);
    state.mouse_captured = captured;
    if (captured) {
        m_captured_viewport = viewport_id;
    } else if (m_captured_viewport == viewport_id) {
        m_captured_viewport = editor::kInvalidViewportId;
    }
}

const ViewportInteractionState* ViewportInteractionTracker::find(editor::ViewportId viewport_id) const noexcept
{
    const auto it = m_states.find(viewport_id);
    return it != m_states.end() ? &it->second : nullptr;
}

bool ViewportInteractionTracker::isHovered(editor::ViewportId viewport_id) const noexcept
{
    const ViewportInteractionState* state = find(viewport_id);
    return state != nullptr && state->hovered && m_hovered_viewport == viewport_id;
}

bool ViewportInteractionTracker::isActive(editor::ViewportId viewport_id) const noexcept
{
    const ViewportInteractionState* state = find(viewport_id);
    return state != nullptr && state->active && m_active_viewport == viewport_id;
}

bool ViewportInteractionTracker::isClicked(editor::ViewportId viewport_id) const noexcept
{
    const ViewportInteractionState* state = find(viewport_id);
    return state != nullptr && state->clicked && isHovered(viewport_id);
}

bool ViewportInteractionTracker::isDoubleClicked(editor::ViewportId viewport_id) const noexcept
{
    const ViewportInteractionState* state = find(viewport_id);
    return state != nullptr && state->double_clicked && isHovered(viewport_id);
}

bool ViewportInteractionTracker::hasMouseCapture(editor::ViewportId viewport_id) const noexcept
{
    const ViewportInteractionState* state = find(viewport_id);
    return state != nullptr && state->mouse_captured && m_captured_viewport == viewport_id;
}

bool ViewportInteractionTracker::allowsInput(editor::ViewportId viewport_id) const noexcept
{
    return isHovered(viewport_id) || hasMouseCapture(viewport_id);
}

editor::ViewportId ViewportInteractionTracker::hoveredViewport() const noexcept
{
    return m_hovered_viewport;
}

editor::ViewportId ViewportInteractionTracker::activeViewport() const noexcept
{
    return m_active_viewport;
}

editor::ViewportId ViewportInteractionTracker::capturedViewport() const noexcept
{
    return m_captured_viewport;
}

ViewportInteractionState& ViewportInteractionTracker::stateFor(editor::ViewportId viewport_id)
{
    auto [it, inserted] = m_states.try_emplace(viewport_id);
    if (inserted) {
        it->second.viewport_id = viewport_id;
    }
    return it->second;
}

} // namespace luna
