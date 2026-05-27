#pragma once

#include "Scene/Scene.h"

#include <cstdint>

#include <string>

namespace luna {

enum class EditorDocumentKind : uint8_t {
    AuthoringScene,
    RuntimeSceneSnapshot,
};

class EditorDocumentContext final {
public:
    EditorDocumentContext() = default;

    EditorDocumentContext(std::string stable_id, EditorDocumentKind kind, bool editable) noexcept
        : m_stable_id(std::move(stable_id)),
          m_kind(kind),
          m_editable(editable)
    {}

    const std::string& stableId() const noexcept
    {
        return m_stable_id;
    }

    EditorDocumentKind kind() const noexcept
    {
        return m_kind;
    }

    bool isRuntime() const noexcept
    {
        return m_kind == EditorDocumentKind::RuntimeSceneSnapshot;
    }

    bool isEditable() const noexcept
    {
        return m_editable;
    }

    void setEditable(bool editable) noexcept
    {
        m_editable = editable;
    }

    bool isRunning() const noexcept
    {
        return m_running;
    }

    void setRunning(bool running) noexcept
    {
        m_running = running;
    }

    void bindScene(Scene* scene) noexcept
    {
        m_scene = scene;
    }

    Scene* scene() noexcept
    {
        return m_scene;
    }

    const Scene* scene() const noexcept
    {
        return m_scene;
    }

    bool hasScene() const noexcept
    {
        return m_scene != nullptr;
    }

private:
    std::string m_stable_id;
    EditorDocumentKind m_kind{EditorDocumentKind::AuthoringScene};
    Scene* m_scene{nullptr};
    bool m_editable{true};
    bool m_running{false};
};

} // namespace luna
