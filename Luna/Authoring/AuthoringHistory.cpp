#include "AuthoringHistory.h"

#include "Scene/SceneSerializer.h"

#include <utility>

namespace luna::authoring {

bool AuthoringHistory::beginTransaction(std::string name,
                                        const Scene& scene,
                                        const std::filesystem::path& scene_file_path,
                                        bool scene_dirty)
{
    if (m_open_transaction.has_value()) {
        return false;
    }

    m_open_transaction = AuthoringHistoryEntry{
        .name = std::move(name),
        .before = captureState(scene, scene_file_path, scene_dirty),
    };
    return true;
}

bool AuthoringHistory::commitTransaction(const Scene& scene,
                                         const std::filesystem::path& scene_file_path,
                                         bool scene_dirty)
{
    if (!m_open_transaction.has_value()) {
        return false;
    }

    m_open_transaction->after = captureState(scene, scene_file_path, scene_dirty);
    if (statesEquivalent(m_open_transaction->before, m_open_transaction->after)) {
        m_open_transaction.reset();
        return false;
    }

    m_undo_stack.push_back(std::move(*m_open_transaction));
    m_open_transaction.reset();
    m_redo_stack.clear();
    trimUndoStack();
    return true;
}

std::optional<AuthoringSceneState> AuthoringHistory::rollbackTransaction()
{
    if (!m_open_transaction.has_value()) {
        return std::nullopt;
    }

    AuthoringSceneState state = cloneState(m_open_transaction->before);
    m_open_transaction.reset();
    return state;
}

std::optional<AuthoringSceneState> AuthoringHistory::undo()
{
    if (m_open_transaction.has_value() || m_undo_stack.empty()) {
        return std::nullopt;
    }

    AuthoringHistoryEntry entry = std::move(m_undo_stack.back());
    m_undo_stack.pop_back();

    AuthoringSceneState state = cloneState(entry.before);
    m_redo_stack.push_back(std::move(entry));
    return state;
}

std::optional<AuthoringSceneState> AuthoringHistory::redo()
{
    if (m_open_transaction.has_value() || m_redo_stack.empty()) {
        return std::nullopt;
    }

    AuthoringHistoryEntry entry = std::move(m_redo_stack.back());
    m_redo_stack.pop_back();

    AuthoringSceneState state = cloneState(entry.after);
    m_undo_stack.push_back(std::move(entry));
    return state;
}

void AuthoringHistory::clear()
{
    m_open_transaction.reset();
    m_undo_stack.clear();
    m_redo_stack.clear();
}

void AuthoringHistory::markCurrentStateSaved(const Scene& scene, const std::filesystem::path& scene_file_path)
{
    if (m_open_transaction.has_value() || m_undo_stack.empty()) {
        return;
    }

    m_undo_stack.back().after = captureState(scene, scene_file_path, false);
}

bool AuthoringHistory::hasOpenTransaction() const noexcept
{
    return m_open_transaction.has_value();
}

bool AuthoringHistory::canUndo() const noexcept
{
    return !m_open_transaction.has_value() && !m_undo_stack.empty();
}

bool AuthoringHistory::canRedo() const noexcept
{
    return !m_open_transaction.has_value() && !m_redo_stack.empty();
}

size_t AuthoringHistory::undoDepth() const noexcept
{
    return m_undo_stack.size();
}

size_t AuthoringHistory::redoDepth() const noexcept
{
    return m_redo_stack.size();
}

AuthoringSceneState AuthoringHistory::captureState(const Scene& scene,
                                                   const std::filesystem::path& scene_file_path,
                                                   bool scene_dirty)
{
    return {
        .scene = scene.clone(),
        .scene_file_path = scene_file_path,
        .scene_dirty = scene_dirty,
    };
}

AuthoringSceneState AuthoringHistory::cloneState(const AuthoringSceneState& state)
{
    return {
        .scene = state.scene ? state.scene->clone() : nullptr,
        .scene_file_path = state.scene_file_path,
        .scene_dirty = state.scene_dirty,
    };
}

bool AuthoringHistory::statesEquivalent(const AuthoringSceneState& lhs, const AuthoringSceneState& rhs)
{
    if (lhs.scene_file_path != rhs.scene_file_path || lhs.scene_dirty != rhs.scene_dirty) {
        return false;
    }

    if (lhs.scene == nullptr || rhs.scene == nullptr) {
        return lhs.scene == nullptr && rhs.scene == nullptr;
    }

    return SceneSerializer::serializeToString(*lhs.scene) == SceneSerializer::serializeToString(*rhs.scene);
}

void AuthoringHistory::trimUndoStack()
{
    while (m_undo_stack.size() > m_max_entries) {
        m_undo_stack.erase(m_undo_stack.begin());
    }
}

} // namespace luna::authoring
