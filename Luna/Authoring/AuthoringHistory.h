#pragma once

#include "Scene/Scene.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace luna::authoring {

struct AuthoringSceneState {
    std::unique_ptr<Scene> scene;
    std::filesystem::path scene_file_path;
    bool scene_dirty{false};
};

struct AuthoringHistoryEntry {
    std::string name;
    AuthoringSceneState before;
    AuthoringSceneState after;
};

class AuthoringHistory {
public:
    [[nodiscard]] bool beginTransaction(std::string name,
                                        const Scene& scene,
                                        const std::filesystem::path& scene_file_path,
                                        bool scene_dirty);
    [[nodiscard]] bool commitTransaction(const Scene& scene,
                                         const std::filesystem::path& scene_file_path,
                                         bool scene_dirty);
    [[nodiscard]] std::optional<AuthoringSceneState> rollbackTransaction();

    [[nodiscard]] std::optional<AuthoringSceneState> undo();
    [[nodiscard]] std::optional<AuthoringSceneState> redo();

    void clear();
    void markCurrentStateSaved(const Scene& scene, const std::filesystem::path& scene_file_path);

    [[nodiscard]] bool hasOpenTransaction() const noexcept;
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] size_t undoDepth() const noexcept;
    [[nodiscard]] size_t redoDepth() const noexcept;

private:
    [[nodiscard]] static AuthoringSceneState captureState(const Scene& scene,
                                                          const std::filesystem::path& scene_file_path,
                                                          bool scene_dirty);
    [[nodiscard]] static AuthoringSceneState cloneState(const AuthoringSceneState& state);
    [[nodiscard]] static bool statesEquivalent(const AuthoringSceneState& lhs, const AuthoringSceneState& rhs);
    void trimUndoStack();

private:
    std::optional<AuthoringHistoryEntry> m_open_transaction;
    std::vector<AuthoringHistoryEntry> m_undo_stack;
    std::vector<AuthoringHistoryEntry> m_redo_stack;
    size_t m_max_entries{100};
};

} // namespace luna::authoring
