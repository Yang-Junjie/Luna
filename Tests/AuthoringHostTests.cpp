#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Authoring/AuthoringHost.h"
#include "Authoring/AuthoringSession.h"
#include "Core/Log.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <iostream>
#include <string_view>

namespace {

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (condition) {
            return true;
        }

        ++m_failures;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    [[nodiscard]] int result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures{0};
};

void testHostKeepsAliasesAcrossPlans(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringHost host(session);

    luna::authoring::AuthoringPlan create_plan;
    create_plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreatePrimitive,
        .alias = "Box",
        .mesh = "Cube",
    });

    luna::authoring::AuthoringReport create_report;
    context.expect(host.executePlan(create_plan, create_report), "host should execute create plan");
    context.expect(create_report.entities.size() == 1, "create plan should bind one alias");
    context.expect(create_report.scene.entity_count == 1, "scene should contain created primitive");

    luna::authoring::AuthoringPlan rename_plan;
    rename_plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::Rename,
        .name = "Persistent Alias Box",
        .entity = {.value = "Box"},
    });
    rename_plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::VerifyEntityExists,
        .entity = {.value = "Box"},
    });
    rename_plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::Snapshot,
    });

    luna::authoring::AuthoringReport rename_report;
    context.expect(host.executePlan(rename_plan, rename_report), "host should resolve alias from an earlier plan");
    context.expect(rename_report.verifications.size() == 1, "rename plan should report one verification");
    context.expect(!rename_report.inspections.empty(), "rename plan should include snapshot inspection");

    const auto view = scene.entityManager().registry().view<luna::TagComponent>();
    const auto found = std::find_if(view.begin(), view.end(), [&](const auto entity_handle) {
        luna::Entity entity(entity_handle, &scene.entityManager());
        return entity.getName() == "Persistent Alias Box";
    });
    context.expect(found != view.end(), "renamed entity should be present in the scene");
}

void testHostClearsAliasesOnNewScene(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringHost host(session);

    luna::authoring::AuthoringPlan create_plan;
    create_plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreatePrimitive,
        .alias = "Box",
        .mesh = "Cube",
    });

    luna::authoring::AuthoringReport create_report;
    context.expect(host.executePlan(create_plan, create_report), "host should execute create plan before reset");

    luna::authoring::AuthoringPlan reset_plan;
    reset_plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    luna::authoring::AuthoringReport reset_report;
    context.expect(host.executePlan(reset_plan, reset_report), "host should execute new scene plan");

    luna::authoring::AuthoringPlan stale_alias_plan;
    stale_alias_plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::VerifyEntityExists,
        .entity = {.value = "Box"},
    });

    luna::authoring::AuthoringReport stale_alias_report;
    context.expect(!host.executePlan(stale_alias_plan, stale_alias_report), "new scene should clear old aliases");
    context.expect(!stale_alias_report.diagnostics.empty(), "stale alias should report diagnostics");
}

void testHostUndoRedoAndEvents(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringHost host(session);

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreatePrimitive,
        .alias = "Box",
        .mesh = "Cube",
    });

    luna::authoring::AuthoringReport report;
    context.expect(host.executePlan(plan, report), "host should execute plan before undo");
    context.expect(host.canUndo(), "host should expose undo availability");
    context.expect(!host.consumeEvents().empty(), "host should expose authoring events");

    context.expect(host.undo(), "host should undo the executed plan");
    context.expect(host.canRedo(), "host should expose redo availability");
    context.expect(host.captureSceneSnapshot().entity_count == 0, "undo should restore empty scene");
    const auto undo_events = host.consumeEvents();
    context.expect(std::any_of(undo_events.begin(), undo_events.end(), [](const auto& event) {
                       return event.type == luna::authoring::AuthoringEventType::HistoryChanged;
                   }),
                   "undo should emit a history event");

    context.expect(host.redo(), "host should redo the executed plan");
    context.expect(host.captureSceneSnapshot().entity_count == 1, "redo should restore created entity");
}

void testHostSessionTransactions(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringHost host(session);

    const auto initial_state = host.sessionState();
    context.expect(initial_state.has_scene, "host should report a bound scene");
    context.expect(!initial_state.has_open_transaction, "host should start without an open transaction");
    context.expect(!initial_state.can_undo, "host should start without undo history");
    context.expect(!initial_state.can_redo, "host should start without redo history");

    context.expect(host.beginTransaction("AI Session"), "host should open a session transaction");
    const auto open_state = host.sessionState();
    context.expect(open_state.has_open_transaction, "host should report the open transaction");
    context.expect(!open_state.can_undo, "open transaction should disable undo");
    context.expect(!open_state.can_redo, "open transaction should disable redo");

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreatePrimitive,
        .alias = "SessionCube",
        .mesh = "Cube",
    });
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::Snapshot});

    luna::authoring::AuthoringReport report;
    context.expect(host.executePlan(plan, report), "host should execute plan inside open transaction");
    context.expect(report.scene.entity_count == 3, "open transaction plan should author bootstrap scene");
    context.expect(host.sessionState().has_open_transaction, "transaction should remain open after executePlan");

    context.expect(host.rollbackTransaction(), "host should rollback the open session transaction");
    const auto rolled_back_state = host.sessionState();
    context.expect(!rolled_back_state.has_open_transaction, "rollback should close the transaction");
    context.expect(!rolled_back_state.can_undo, "rolled back transaction should not produce undo history");
    context.expect(host.captureSceneSnapshot().entity_count == 0, "rollback should restore empty scene");

    context.expect(host.beginTransaction("AI Session Commit"), "host should open a second session transaction");
    context.expect(host.executePlan(plan, report), "host should execute plan for commit path");
    context.expect(host.commitTransaction(), "host should commit the open session transaction");

    const auto committed_state = host.sessionState();
    context.expect(!committed_state.has_open_transaction, "commit should close the transaction");
    context.expect(committed_state.can_undo, "commit should publish undo history");
    context.expect(committed_state.undo_depth == 1, "commit should create one undo entry");
    context.expect(committed_state.scene.entity_count == 3, "committed session should keep authored scene");
}

void testHostCapabilities(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringHost host(session);

    const auto capabilities = host.capabilities();
    context.expect(!capabilities.empty(), "host should expose authoring capabilities");
    context.expect(std::any_of(capabilities.begin(), capabilities.end(), [](const auto& capability) {
                       return capability.op == "snapshot";
                   }),
                   "host capabilities should include snapshot");
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);
    luna::AssetManager::get().init();

    TestContext context;
    testHostKeepsAliasesAcrossPlans(context);
    testHostClearsAliasesOnNewScene(context);
    testHostUndoRedoAndEvents(context);
    testHostSessionTransactions(context);
    testHostCapabilities(context);

    luna::AssetManager::get().clear();
    luna::AssetDatabase::clear();
    luna::Logger::shutdown();
    return context.result();
}
