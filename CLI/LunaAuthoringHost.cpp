#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Authoring/AuthoringCapabilities.h"
#include "Authoring/AuthoringHost.h"
#include "Authoring/AuthoringJson.h"
#include "Authoring/AuthoringPlanJson.h"
#include "Authoring/AuthoringSession.h"
#include "Core/Log.h"
#include "Scene/Scene.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using Json = nlohmann::ordered_json;

constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;

struct HostState {
    luna::Scene scene;
    luna::authoring::AuthoringSession session{scene};
    luna::authoring::AuthoringHost host{session};
    bool running{true};
};

const char* eventTypeName(luna::authoring::AuthoringEventType type) noexcept
{
    using EventType = luna::authoring::AuthoringEventType;
    switch (type) {
        case EventType::SceneReset:
            return "sceneReset";
        case EventType::SceneCreated:
            return "sceneCreated";
        case EventType::SceneLoaded:
            return "sceneLoaded";
        case EventType::SceneSaved:
            return "sceneSaved";
        case EventType::SceneDirtyChanged:
            return "sceneDirtyChanged";
        case EventType::SceneSettingsChanged:
            return "sceneSettingsChanged";
        case EventType::HistoryChanged:
            return "historyChanged";
        case EventType::EntityCreated:
            return "entityCreated";
        case EventType::EntityModified:
            return "entityModified";
        case EventType::EntityDestroyed:
            return "entityDestroyed";
        case EventType::EntityReparented:
            return "entityReparented";
        case EventType::ComponentAdded:
            return "componentAdded";
        case EventType::ComponentRemoved:
            return "componentRemoved";
    }

    return "unknown";
}

Json sceneSnapshotJson(const luna::authoring::AuthoringSceneSnapshot& snapshot)
{
    Json result = Json::object();
    result["name"] = snapshot.name;
    result["path"] = snapshot.path.empty() ? Json(nullptr) : Json(snapshot.path.string());
    result["entityCount"] = snapshot.entity_count;
    result["dirty"] = snapshot.dirty;
    return result;
}

Json eventJson(const luna::authoring::AuthoringEvent& event)
{
    Json result = Json::object();
    result["type"] = eventTypeName(event.type);
    result["entity"] = event.entity_id.isValid() ? Json(event.entity_id.toString()) : Json(nullptr);
    result["path"] = event.path.empty() ? Json(nullptr) : Json(event.path.string());
    result["message"] = event.message;
    return result;
}

Json eventsJson(std::vector<luna::authoring::AuthoringEvent> events)
{
    Json result = Json::array();
    for (const luna::authoring::AuthoringEvent& event : events) {
        result.push_back(eventJson(event));
    }
    return result;
}

Json sessionStateJson(const luna::authoring::AuthoringHostSessionState& state)
{
    Json result = Json::object();
    result["hasScene"] = state.has_scene;
    result["scene"] = sceneSnapshotJson(state.scene);
    result["hasOpenTransaction"] = state.has_open_transaction;
    result["canUndo"] = state.can_undo;
    result["canRedo"] = state.can_redo;
    result["undoDepth"] = state.undo_depth;
    result["redoDepth"] = state.redo_depth;
    return result;
}

Json authoringReportJson(const luna::authoring::AuthoringReport& report, bool ok)
{
    std::ostringstream stream;
    luna::authoring::writeAuthoringReportJson(stream, report, ok);
    return Json::parse(stream.str());
}

Json capabilitiesJson()
{
    std::ostringstream stream;
    luna::authoring::writeDefaultAuthoringCapabilitiesJson(stream);
    return Json::parse(stream.str());
}

Json sessionJson(HostState& state)
{
    return sessionStateJson(state.host.sessionState());
}

Json transactionResultJson(HostState& state, bool ok)
{
    Json result = Json::object();
    result["ok"] = ok;
    result["scene"] = sceneSnapshotJson(state.host.captureSceneSnapshot());
    result["events"] = eventsJson(state.host.consumeEvents());
    return result;
}

Json makeResponse(const Json& id, Json result)
{
    Json response = Json::object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = std::move(result);
    return response;
}

Json makeError(const Json& id, int code, std::string message, Json data = nullptr)
{
    Json error = Json::object();
    error["code"] = code;
    error["message"] = std::move(message);
    if (!data.is_null()) {
        error["data"] = std::move(data);
    }

    Json response = Json::object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = std::move(error);
    return response;
}

bool loadPlanFromJson(const Json& input,
                      luna::authoring::AuthoringPlan& plan,
                      Json& error_data)
{
    std::istringstream stream(input.dump());
    std::vector<std::string> errors;
    std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
    if (luna::authoring::loadAuthoringPlanJson(stream, plan, errors, &diagnostics)) {
        return true;
    }

    error_data = Json::object();
    error_data["errors"] = Json::array();
    for (const std::string& error : errors) {
        error_data["errors"].push_back(error);
    }

    Json diagnostic_messages = Json::array();
    for (const luna::authoring::AuthoringDiagnostic& diagnostic : diagnostics) {
        diagnostic_messages.push_back(diagnostic.message);
    }
    error_data["diagnostics"] = std::move(diagnostic_messages);
    return false;
}

Json executePlan(HostState& state, const luna::authoring::AuthoringPlan& plan)
{
    luna::authoring::AuthoringReport report;
    const bool ok = state.host.executePlan(plan, report);

    Json result = Json::object();
    result["ok"] = ok;
    result["report"] = authoringReportJson(report, ok);
    result["events"] = eventsJson(state.host.consumeEvents());
    return result;
}

Json handleExecutePlan(HostState& state, const Json& params, const Json& id)
{
    if (!params.is_object()) {
        return makeError(id, kInvalidParams, "executePlan params must be an object.");
    }

    const Json& plan_json = params.contains("plan") ? params.at("plan") : params;
    luna::authoring::AuthoringPlan plan;
    Json error_data;
    if (!loadPlanFromJson(plan_json, plan, error_data)) {
        return makeError(id, kInvalidParams, "Invalid authoring plan.", std::move(error_data));
    }

    return makeResponse(id, executePlan(state, plan));
}

Json handleSnapshot(HostState& state, const Json& id)
{
    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::Snapshot});
    return makeResponse(id, executePlan(state, plan));
}

Json handleUndo(HostState& state, const Json& id)
{
    const bool changed = state.host.undo();
    Json result = Json::object();
    result["ok"] = changed;
    result["scene"] = sceneSnapshotJson(state.host.captureSceneSnapshot());
    result["events"] = eventsJson(state.host.consumeEvents());
    return makeResponse(id, std::move(result));
}

Json handleRedo(HostState& state, const Json& id)
{
    const bool changed = state.host.redo();
    Json result = Json::object();
    result["ok"] = changed;
    result["scene"] = sceneSnapshotJson(state.host.captureSceneSnapshot());
    result["events"] = eventsJson(state.host.consumeEvents());
    return makeResponse(id, std::move(result));
}

Json handleBeginTransaction(HostState& state, const Json& params, const Json& id)
{
    if (!params.is_object()) {
        return makeError(id, kInvalidParams, "beginTransaction params must be an object.");
    }

    if (!params.contains("name") || !params.at("name").is_string()) {
        return makeError(id, kInvalidParams, "beginTransaction requires a non-empty 'name'.");
    }

    const std::string name = params.at("name").get<std::string>();
    if (name.empty()) {
        return makeError(id, kInvalidParams, "beginTransaction requires a non-empty 'name'.");
    }

    Json result = Json::object();
    result["ok"] = state.host.beginTransaction(name);
    result["session"] = sessionJson(state);
    return makeResponse(id, std::move(result));
}

Json handleCommitTransaction(HostState& state, const Json& id)
{
    return makeResponse(id, transactionResultJson(state, state.host.commitTransaction()));
}

Json handleRollbackTransaction(HostState& state, const Json& id)
{
    return makeResponse(id, transactionResultJson(state, state.host.rollbackTransaction()));
}

Json handleSession(HostState& state, const Json& id)
{
    return makeResponse(id, sessionJson(state));
}

Json handleClearHistory(HostState& state, const Json& id)
{
    state.host.clearHistory();
    Json result = Json::object();
    result["ok"] = true;
    result["session"] = sessionJson(state);
    return makeResponse(id, std::move(result));
}

Json dispatchRequest(HostState& state, const Json& request)
{
    const Json id = request.contains("id") ? request.at("id") : Json(nullptr);
    if (!request.is_object() || request.value("jsonrpc", "") != "2.0" || !request.contains("method") ||
        !request.at("method").is_string()) {
        return makeError(id, kInvalidRequest, "Invalid JSON-RPC request.");
    }

    const std::string method = request.at("method").get<std::string>();
    const Json params = request.contains("params") ? request.at("params") : Json::object();

    if (method == "capabilities") {
        return makeResponse(id, capabilitiesJson());
    }
    if (method == "executePlan") {
        return handleExecutePlan(state, params, id);
    }
    if (method == "session") {
        return handleSession(state, id);
    }
    if (method == "beginTransaction") {
        return handleBeginTransaction(state, params, id);
    }
    if (method == "commitTransaction") {
        return handleCommitTransaction(state, id);
    }
    if (method == "rollbackTransaction") {
        return handleRollbackTransaction(state, id);
    }
    if (method == "snapshot") {
        return handleSnapshot(state, id);
    }
    if (method == "undo") {
        return handleUndo(state, id);
    }
    if (method == "redo") {
        return handleRedo(state, id);
    }
    if (method == "events") {
        Json result = Json::object();
        result["events"] = eventsJson(state.host.consumeEvents());
        return makeResponse(id, std::move(result));
    }
    if (method == "clearAliases") {
        state.host.clearAliases();
        Json result = Json::object();
        result["ok"] = true;
        return makeResponse(id, std::move(result));
    }
    if (method == "clearHistory") {
        return handleClearHistory(state, id);
    }
    if (method == "shutdown") {
        state.running = false;
        Json result = Json::object();
        result["ok"] = true;
        return makeResponse(id, std::move(result));
    }

    return makeError(id, kMethodNotFound, "Unknown method '" + method + "'.");
}

void writeJsonLine(const Json& value)
{
    std::cout << value.dump(-1, ' ', false, Json::error_handler_t::replace) << '\n';
    std::cout.flush();
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Off);
    luna::AssetManager::get().init();

    HostState state;
    std::string line;
    while (state.running && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        try {
            writeJsonLine(dispatchRequest(state, Json::parse(line)));
        } catch (const Json::parse_error& error) {
            writeJsonLine(makeError(nullptr, kParseError, error.what()));
        } catch (const std::exception& error) {
            writeJsonLine(makeError(nullptr, kInternalError, error.what()));
        } catch (...) {
            writeJsonLine(makeError(nullptr, kInternalError, "Unknown internal error."));
        }
    }

    luna::AssetManager::get().clear();
    luna::AssetDatabase::clear();
    luna::Logger::shutdown();
    return 0;
}
