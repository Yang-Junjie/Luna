#include "Protocol/ViewProtocolJson.h"

#include <exception>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Json = luna::editor::Json;

constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;

[[nodiscard]] luna::editor::EditorViewportState createViewportState(luna::editor::EditorViewportId viewport_id,
                                                                     luna::editor::EditorRenderPlaneId plane_id);

struct ViewHostState {
    std::map<luna::editor::EditorViewportId, luna::editor::EditorViewportState> viewports;
    luna::editor::EditorViewportId active_viewport_id{1};
    bool running{true};

    ViewHostState()
    {
        viewports.emplace(1, createViewportState(1, 1));
    }
};

[[nodiscard]] luna::editor::EditorViewportState createViewportState(luna::editor::EditorViewportId viewport_id,
                                                                     luna::editor::EditorRenderPlaneId plane_id)
{
    luna::editor::EditorViewportState state;
    state.viewport_id = viewport_id;
    state.render_plane.descriptor.viewport_id = viewport_id;
    state.render_plane.descriptor.plane_id = plane_id;
    state.render_plane.descriptor.label = state.title;
    return state;
}

[[nodiscard]] luna::editor::EditorViewportState& ensureViewport(ViewHostState& state,
                                                                luna::editor::EditorViewportId viewport_id)
{
    const luna::editor::EditorViewportId resolved_id = viewport_id != 0
                                                           ? viewport_id
                                                           : (state.active_viewport_id != 0 ? state.active_viewport_id
                                                                                           : luna::editor::EditorViewportId{1});
    const auto [it, inserted] = state.viewports.try_emplace(resolved_id, createViewportState(resolved_id, resolved_id));
    (void) inserted;
    return it->second;
}

[[nodiscard]] Json editorViewDocumentJson(const ViewHostState& state)
{
    Json document = Json::object();
    document["protocol"] = luna::editor::editorProtocolJson();
    document["activeViewportId"] = state.active_viewport_id == 0 ? Json(nullptr) : Json(state.active_viewport_id);

    Json viewports = Json::object();
    for (const auto& [viewport_id, viewport_state] : state.viewports) {
        viewports[std::to_string(viewport_id)] = luna::editor::editorViewportStateJson(viewport_state);
    }
    document["viewports"] = std::move(viewports);
    return document;
}

[[nodiscard]] Json makeResponse(const Json& id, Json result)
{
    Json response = Json::object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = std::move(result);
    return response;
}

[[nodiscard]] Json makeError(const Json& id, int code, std::string message, Json data = nullptr)
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

[[nodiscard]] bool readNonNegativeIntegerField(const Json& node,
                                               const char* key,
                                               uint64_t& value,
                                               std::string_view field_name,
                                               std::vector<std::string>* errors)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        if (errors != nullptr) {
            errors->push_back("Field '" + std::string(field_name) + "' is required.");
        }
        return false;
    }

    if (!it->is_number_integer() && !it->is_number_unsigned()) {
        if (errors != nullptr) {
            errors->push_back("Field '" + std::string(field_name) + "' must be a non-negative integer.");
        }
        return false;
    }

    try {
        value = it->get<uint64_t>();
    } catch (const std::exception&) {
        if (errors != nullptr) {
            errors->push_back("Field '" + std::string(field_name) + "' must be a non-negative integer.");
        }
        return false;
    }

    return true;
}

[[nodiscard]] bool readCommandParams(const Json& params,
                                     luna::editor::EditorViewportCommand& command,
                                     Json& error_data)
{
    const Json& command_json = params.contains("command") ? params.at("command") : params;
    std::vector<std::string> errors;
    if (luna::editor::editorViewportCommandFromJson(command_json, command, &errors)) {
        return true;
    }

    error_data = Json::object();
    error_data["errors"] = Json::array();
    for (const std::string& error : errors) {
        error_data["errors"].push_back(error);
    }
    return false;
}

[[nodiscard]] Json handleSnapshot(const ViewHostState& state, const Json& id)
{
    return makeResponse(id, editorViewDocumentJson(state));
}

[[nodiscard]] Json handleApplyViewportCommand(ViewHostState& state, const Json& params, const Json& id)
{
    if (!params.is_object()) {
        return makeError(id, kInvalidParams, "applyViewportCommand params must be an object.");
    }

    luna::editor::EditorViewportCommand command;
    Json error_data;
    if (!readCommandParams(params, command, error_data)) {
        return makeError(id, kInvalidParams, "Invalid viewport command.", std::move(error_data));
    }

    const luna::editor::EditorViewportId viewport_id = command.viewport_id != 0
                                                          ? command.viewport_id
                                                          : (state.active_viewport_id != 0 ? state.active_viewport_id
                                                                                          : luna::editor::EditorViewportId{1});
    luna::editor::EditorViewportState& viewport = ensureViewport(state, viewport_id);
    const luna::editor::EditorViewportCommandResult result = luna::editor::applyEditorViewportCommand(viewport, command);

    if (command.kind == luna::editor::EditorViewportCommandKind::CreateViewport || state.active_viewport_id == 0) {
        state.active_viewport_id = viewport_id;
    }
    if (command.kind == luna::editor::EditorViewportCommandKind::DestroyViewport &&
        state.active_viewport_id == viewport_id) {
        state.active_viewport_id = 0;
    }

    Json response = Json::object();
    response["ok"] = result.accepted;
    response["result"] = luna::editor::editorViewportCommandResultJson(result);
    response["document"] = editorViewDocumentJson(state);
    return makeResponse(id, std::move(response));
}

[[nodiscard]] Json handleBindRenderPlane(ViewHostState& state, const Json& params, const Json& id)
{
    if (!params.is_object()) {
        return makeError(id, kInvalidParams, "bindRenderPlane params must be an object.");
    }

    uint64_t viewport_id = 0;
    std::vector<std::string> errors;
    if (!readNonNegativeIntegerField(params, "viewportId", viewport_id, "viewportId", &errors)) {
        return makeError(id, kInvalidParams, "bindRenderPlane requires a non-negative 'viewportId'.");
    }

    const auto descriptor_node = params.find("descriptor");
    if (descriptor_node == params.end()) {
        return makeError(id, kInvalidParams, "bindRenderPlane requires a 'descriptor'.");
    }

    luna::editor::EditorRenderPlaneDescriptor descriptor;
    if (!luna::editor::editorRenderPlaneDescriptorFromJson(*descriptor_node, descriptor, &errors)) {
        Json error_data = Json::object();
        error_data["errors"] = Json::array();
        for (const std::string& error : errors) {
            error_data["errors"].push_back(error);
        }
        return makeError(id, kInvalidParams, "Invalid render plane descriptor.", std::move(error_data));
    }

    descriptor.viewport_id = viewport_id;
    if (descriptor.plane_id == 0) {
        descriptor.plane_id = viewport_id;
    }

    luna::editor::EditorViewportState& viewport = ensureViewport(state, viewport_id);
    const bool changed = luna::editor::bindEditorRenderPlane(viewport.render_plane, std::move(descriptor));

    Json response = Json::object();
    response["ok"] = true;
    response["changed"] = changed;
    response["document"] = editorViewDocumentJson(state);
    return makeResponse(id, std::move(response));
}

[[nodiscard]] Json handlePresentRenderPlaneFrame(ViewHostState& state, const Json& params, const Json& id)
{
    if (!params.is_object()) {
        return makeError(id, kInvalidParams, "presentRenderPlaneFrame params must be an object.");
    }

    uint64_t viewport_id = 0;
    uint64_t frame_id = 0;
    uint64_t sequence = 0;
    uint64_t timestamp_ns = 0;
    std::vector<std::string> errors;
    if (!readNonNegativeIntegerField(params, "viewportId", viewport_id, "viewportId", &errors) ||
        !readNonNegativeIntegerField(params, "frameId", frame_id, "frameId", &errors) ||
        !readNonNegativeIntegerField(params, "sequence", sequence, "sequence", &errors) ||
        !readNonNegativeIntegerField(params, "timestampNs", timestamp_ns, "timestampNs", &errors)) {
        return makeError(id, kInvalidParams, "presentRenderPlaneFrame requires non-negative viewport/frame values.");
    }

    luna::editor::EditorViewportState& viewport = ensureViewport(state, viewport_id);
    luna::editor::presentEditorRenderPlaneFrame(viewport.render_plane,
                                                static_cast<luna::editor::EditorFrameId>(frame_id),
                                                sequence,
                                                timestamp_ns);

    Json response = Json::object();
    response["ok"] = true;
    response["changed"] = true;
    response["document"] = editorViewDocumentJson(state);
    return makeResponse(id, std::move(response));
}

[[nodiscard]] Json handleReleaseRenderPlane(ViewHostState& state, const Json& params, const Json& id)
{
    if (!params.is_object()) {
        return makeError(id, kInvalidParams, "releaseRenderPlane params must be an object.");
    }

    uint64_t viewport_id = 0;
    std::vector<std::string> errors;
    if (!readNonNegativeIntegerField(params, "viewportId", viewport_id, "viewportId", &errors)) {
        return makeError(id, kInvalidParams, "releaseRenderPlane requires a non-negative 'viewportId'.");
    }

    luna::editor::EditorViewportState& viewport = ensureViewport(state, viewport_id);
    const bool changed = luna::editor::releaseEditorRenderPlane(viewport.render_plane);

    Json response = Json::object();
    response["ok"] = true;
    response["changed"] = changed;
    response["document"] = editorViewDocumentJson(state);
    return makeResponse(id, std::move(response));
}

[[nodiscard]] Json dispatchRequest(ViewHostState& state, const Json& request)
{
    const Json id = request.contains("id") ? request.at("id") : Json(nullptr);
    if (!request.is_object() || request.value("jsonrpc", "") != "2.0" || !request.contains("method") ||
        !request.at("method").is_string()) {
        return makeError(id, kInvalidRequest, "Invalid JSON-RPC request.");
    }

    const std::string method = request.at("method").get<std::string>();
    const Json params = request.contains("params") ? request.at("params") : Json::object();

    if (method == "snapshot") {
        return handleSnapshot(state, id);
    }
    if (method == "applyViewportCommand") {
        return handleApplyViewportCommand(state, params, id);
    }
    if (method == "bindRenderPlane") {
        return handleBindRenderPlane(state, params, id);
    }
    if (method == "presentRenderPlaneFrame") {
        return handlePresentRenderPlaneFrame(state, params, id);
    }
    if (method == "releaseRenderPlane") {
        return handleReleaseRenderPlane(state, params, id);
    }
    if (method == "shutdown") {
        state.running = false;
        Json result = Json::object();
        result["ok"] = true;
        return makeResponse(id, std::move(result));
    }

    return makeError(id, kMethodNotFound, "Unknown method '" + method + "'.");
}

} // namespace

int main()
{
    ViewHostState state;

    try {
        std::string line;
        while (state.running && std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }

            Json request;
            try {
                request = Json::parse(line);
            } catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
                std::cout << makeError(Json(nullptr), kParseError, "Failed to parse JSON request.").dump() << '\n';
                continue;
            }

            try {
                const Json response = dispatchRequest(state, request);
                std::cout << response.dump() << '\n';
                std::cout.flush();
            } catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
                std::cout << makeError(Json(nullptr), -32603, "Internal editor view host error.").dump() << '\n';
                std::cout.flush();
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
