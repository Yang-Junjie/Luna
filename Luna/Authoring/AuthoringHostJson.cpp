#include "AuthoringHostJson.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace luna::authoring {
namespace {

struct AuthoringHostWireMethodSpec {
    std::string_view name;
    std::string_view request_shape;
    std::string_view result_shape;
    std::string_view notes;
};

constexpr std::array<AuthoringHostWireMethodSpec, 13> kAuthoringHostWireMethods{{
    {
        "capabilities",
        R"({ jsonrpc: "2.0", id: number, method: "capabilities" })",
        "AuthoringCapabilitiesDocument",
        "",
    },
    {
        "session",
        R"({ jsonrpc: "2.0", id: number, method: "session" })",
        "AuthoringHostSessionState",
        "",
    },
    {
        "executePlan",
        R"({ jsonrpc: "2.0", id: number, method: "executePlan", params: { plan: AuthoringPlan } })",
        R"({ ok: boolean, report: AuthoringReport, events: AuthoringHostEvent[] })",
        "The host also accepts a legacy bare plan object for compatibility.",
    },
    {
        "beginTransaction",
        R"({ jsonrpc: "2.0", id: number, method: "beginTransaction", params: { name: string } })",
        R"({ ok: boolean, session: AuthoringHostSessionState })",
        "",
    },
    {
        "commitTransaction",
        R"({ jsonrpc: "2.0", id: number, method: "commitTransaction" })",
        R"({ ok: boolean, scene: AuthoringSceneSnapshot, events: AuthoringHostEvent[] })",
        "",
    },
    {
        "rollbackTransaction",
        R"({ jsonrpc: "2.0", id: number, method: "rollbackTransaction" })",
        R"({ ok: boolean, scene: AuthoringSceneSnapshot, events: AuthoringHostEvent[] })",
        "",
    },
    {
        "snapshot",
        R"({ jsonrpc: "2.0", id: number, method: "snapshot" })",
        R"({ ok: boolean, report: AuthoringReport, events: AuthoringHostEvent[] })",
        "",
    },
    {
        "undo",
        R"({ jsonrpc: "2.0", id: number, method: "undo" })",
        R"({ ok: boolean, scene: AuthoringSceneSnapshot, events: AuthoringHostEvent[] })",
        "",
    },
    {
        "redo",
        R"({ jsonrpc: "2.0", id: number, method: "redo" })",
        R"({ ok: boolean, scene: AuthoringSceneSnapshot, events: AuthoringHostEvent[] })",
        "",
    },
    {
        "events",
        R"({ jsonrpc: "2.0", id: number, method: "events" })",
        R"({ events: AuthoringHostEvent[] })",
        "Returns pending events and clears the host-side event queue.",
    },
    {
        "clearAliases",
        R"({ jsonrpc: "2.0", id: number, method: "clearAliases" })",
        R"({ ok: boolean })",
        "",
    },
    {
        "clearHistory",
        R"({ jsonrpc: "2.0", id: number, method: "clearHistory" })",
        R"({ ok: boolean, session: AuthoringHostSessionState })",
        "",
    },
    {
        "shutdown",
        R"({ jsonrpc: "2.0", id: number, method: "shutdown" })",
        R"({ ok: boolean })",
        "Signals the host loop to stop after the current request.",
    },
}};

Json authoringHostWireMethodJson(const AuthoringHostWireMethodSpec& spec)
{
    Json result = Json::object();
    result["name"] = std::string(spec.name);
    result["requestShape"] = std::string(spec.request_shape);
    result["resultShape"] = std::string(spec.result_shape);
    if (!spec.notes.empty()) {
        result["notes"] = std::string(spec.notes);
    }
    return result;
}

} // namespace

std::string_view authoringEventTypeName(AuthoringEventType type) noexcept
{
    switch (type) {
        case AuthoringEventType::SceneReset:
            return "sceneReset";
        case AuthoringEventType::SceneCreated:
            return "sceneCreated";
        case AuthoringEventType::SceneLoaded:
            return "sceneLoaded";
        case AuthoringEventType::SceneSaved:
            return "sceneSaved";
        case AuthoringEventType::SceneDirtyChanged:
            return "sceneDirtyChanged";
        case AuthoringEventType::SceneSettingsChanged:
            return "sceneSettingsChanged";
        case AuthoringEventType::HistoryChanged:
            return "historyChanged";
        case AuthoringEventType::EntityCreated:
            return "entityCreated";
        case AuthoringEventType::EntityModified:
            return "entityModified";
        case AuthoringEventType::EntityDestroyed:
            return "entityDestroyed";
        case AuthoringEventType::EntityReparented:
            return "entityReparented";
        case AuthoringEventType::ComponentAdded:
            return "componentAdded";
        case AuthoringEventType::ComponentRemoved:
            return "componentRemoved";
    }

    return "unknown";
}

Json authoringSceneSnapshotJson(const AuthoringSceneSnapshot& snapshot)
{
    Json result = Json::object();
    result["name"] = snapshot.name;
    result["path"] = snapshot.path.empty() ? Json(nullptr) : Json(snapshot.path.string());
    result["entityCount"] = snapshot.entity_count;
    result["dirty"] = snapshot.dirty;
    return result;
}

Json authoringEventJson(const AuthoringEvent& event)
{
    Json result = Json::object();
    result["type"] = authoringEventTypeName(event.type);
    result["entity"] = event.entity_id.isValid() ? Json(event.entity_id.toString()) : Json(nullptr);
    result["path"] = event.path.empty() ? Json(nullptr) : Json(event.path.string());
    result["message"] = event.message;
    return result;
}

Json authoringEventsJson(const std::vector<AuthoringEvent>& events)
{
    Json result = Json::array();
    for (const AuthoringEvent& event : events) {
        result.push_back(authoringEventJson(event));
    }
    return result;
}

Json authoringHostSessionStateJson(const AuthoringHostSessionState& state)
{
    Json result = Json::object();
    result["hasScene"] = state.has_scene;
    result["scene"] = authoringSceneSnapshotJson(state.scene);
    result["hasOpenTransaction"] = state.has_open_transaction;
    result["canUndo"] = state.can_undo;
    result["canRedo"] = state.can_redo;
    result["undoDepth"] = state.undo_depth;
    result["redoDepth"] = state.redo_depth;
    return result;
}

Json authoringHostWireManifestJson()
{
    Json protocol = Json::object();
    protocol["name"] = "luna.authoring.host";
    protocol["version"] = 1;

    Json methods = Json::array();
    for (const AuthoringHostWireMethodSpec& spec : kAuthoringHostWireMethods) {
        methods.push_back(authoringHostWireMethodJson(spec));
    }

    Json root = Json::object();
    root["protocol"] = std::move(protocol);
    root["transport"] = "stdio-jsonrpc";
    root["framing"] = "newline-delimited";
    root["rpcVersion"] = "2.0";
    root["methods"] = std::move(methods);
    return root;
}

} // namespace luna::authoring
