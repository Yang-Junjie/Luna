#pragma once

#include "Authoring/AuthoringCapabilities.h"
#include "Authoring/AuthoringExecutor.h"
#include "Authoring/AuthoringProtocol.h"
#include "Authoring/AuthoringTypes.h"

#include <vector>

namespace luna::authoring {

struct AuthoringHostSessionState {
    bool has_scene{false};
    AuthoringSceneSnapshot scene;
    bool has_open_transaction{false};
    bool can_undo{false};
    bool can_redo{false};
    size_t undo_depth{0};
    size_t redo_depth{0};
};

class AuthoringHost final {
public:
    explicit AuthoringHost(AuthoringSession& session);

    [[nodiscard]] AuthoringSession& session() noexcept;
    [[nodiscard]] const AuthoringSession& session() const noexcept;

    [[nodiscard]] bool executePlan(const AuthoringPlan& plan, AuthoringReport& report);
    void clearAliases();

    [[nodiscard]] bool beginTransaction(std::string name);
    [[nodiscard]] bool commitTransaction();
    [[nodiscard]] bool rollbackTransaction();
    void clearHistory();

    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

    [[nodiscard]] AuthoringHostSessionState sessionState() const;
    [[nodiscard]] AuthoringSceneSnapshot captureSceneSnapshot() const;
    [[nodiscard]] std::vector<AuthoringEvent> consumeEvents();
    [[nodiscard]] std::vector<AuthoringCapability> capabilities() const;

private:
    AuthoringSession& m_session;
    AuthoringExecutor m_executor;
};

} // namespace luna::authoring
