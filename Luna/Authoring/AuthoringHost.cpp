#include "AuthoringHost.h"

#include "Authoring/AuthoringSession.h"

namespace luna::authoring {

AuthoringHost::AuthoringHost(AuthoringSession& session)
    : m_session(session),
      m_executor(session)
{}

AuthoringSession& AuthoringHost::session() noexcept
{
    return m_session;
}

const AuthoringSession& AuthoringHost::session() const noexcept
{
    return m_session;
}

bool AuthoringHost::executePlan(const AuthoringPlan& plan, AuthoringReport& report)
{
    return m_executor.execute(plan, report);
}

void AuthoringHost::clearAliases()
{
    m_executor.clearAliases();
}

bool AuthoringHost::beginTransaction(std::string name)
{
    return m_session.beginTransaction(std::move(name));
}

bool AuthoringHost::commitTransaction()
{
    return m_session.commitTransaction();
}

bool AuthoringHost::rollbackTransaction()
{
    return m_session.rollbackTransaction();
}

void AuthoringHost::clearHistory()
{
    m_session.clearHistory();
}

bool AuthoringHost::undo()
{
    return m_session.undo();
}

bool AuthoringHost::redo()
{
    return m_session.redo();
}

bool AuthoringHost::canUndo() const noexcept
{
    return m_session.canUndo();
}

bool AuthoringHost::canRedo() const noexcept
{
    return m_session.canRedo();
}

AuthoringHostSessionState AuthoringHost::sessionState() const
{
    AuthoringHostSessionState state{};
    state.has_scene = m_session.hasScene();
    if (state.has_scene) {
        state.scene = captureAuthoringSceneSnapshot(m_session);
    }
    state.has_open_transaction = m_session.hasOpenTransaction();
    state.can_undo = m_session.canUndo();
    state.can_redo = m_session.canRedo();
    state.undo_depth = m_session.undoDepth();
    state.redo_depth = m_session.redoDepth();
    return state;
}

AuthoringSceneSnapshot AuthoringHost::captureSceneSnapshot() const
{
    return captureAuthoringSceneSnapshot(m_session);
}

std::vector<AuthoringEvent> AuthoringHost::consumeEvents()
{
    return m_session.consumeEvents();
}

std::vector<AuthoringCapability> AuthoringHost::capabilities() const
{
    return defaultAuthoringCapabilities();
}

} // namespace luna::authoring
