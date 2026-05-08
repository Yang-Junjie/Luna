#include "AuthoringPlanJson.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace luna::authoring {
namespace {

using Json = nlohmann::ordered_json;

struct PlanJsonContext {
    std::vector<std::string>& errors;
    std::vector<AuthoringDiagnostic>* diagnostics;
    const std::filesystem::path& source_path;
};

std::string commandFieldName(size_t command_index, std::string_view field_name)
{
    return "commands[" + std::to_string(command_index) + "]." + std::string(field_name);
}

void appendPlanDiagnostic(const PlanJsonContext& context,
                          AuthoringDiagnosticCode code,
                          AuthoringDiagnosticPhase phase,
                          std::string message,
                          std::string field = {},
                          size_t command_index = 0,
                          bool has_command_index = false,
                          std::string command = {},
                          std::string expected = {},
                          std::string actual = {},
                          bool recoverable = true)
{
    context.errors.push_back(message);
    if (context.diagnostics == nullptr) {
        return;
    }

    AuthoringDiagnostic diagnostic;
    diagnostic.severity = AuthoringDiagnosticSeverity::Error;
    diagnostic.phase = phase;
    diagnostic.code = code;
    diagnostic.recoverable = recoverable;
    diagnostic.command = std::move(command);
    diagnostic.field = std::move(field);
    diagnostic.expected = std::move(expected);
    diagnostic.actual = std::move(actual);
    diagnostic.message = std::move(message);
    if (!context.source_path.empty()) {
        diagnostic.path = context.source_path;
    }
    if (has_command_index) {
        diagnostic.has_command_index = true;
        diagnostic.command_index = command_index;
    }

    context.diagnostics->push_back(std::move(diagnostic));
}

bool readStringValue(const PlanJsonContext& context,
                     const Json& node,
                     const std::string& field_name,
                     std::string& value,
                     size_t command_index = 0,
                     bool has_command_index = false,
                     std::string_view command = {})
{
    if (!node.is_string()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a non-empty string.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "non-empty string");
        return false;
    }

    try {
        value = node.get<std::string>();
    } catch (const std::exception&) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a non-empty string.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "non-empty string");
        return false;
    }

    if (value.empty()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a non-empty string.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "non-empty string",
                             "empty string");
        return false;
    }

    return true;
}

bool readStringField(const PlanJsonContext& context,
                     const Json& node,
                     const char* key,
                     const std::string& field_name,
                     std::string& value,
                     size_t command_index = 0,
                     bool has_command_index = false,
                     std::string_view command = {})
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a non-empty string.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "non-empty string",
                             "missing");
        return false;
    }

    return readStringValue(context, *it, field_name, value, command_index, has_command_index, command);
}

bool readNumberValue(const PlanJsonContext& context,
                     const Json& node,
                     const std::string& field_name,
                     double& value,
                     size_t command_index = 0,
                     bool has_command_index = false,
                     std::string_view command = {})
{
    if (!node.is_number()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidNumber,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a finite number.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "finite number");
        return false;
    }

    try {
        value = node.get<double>();
    } catch (const std::exception&) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidNumber,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a finite number.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "finite number");
        return false;
    }

    if (!std::isfinite(value)) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidNumber,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a finite number.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "finite number");
        return false;
    }

    return true;
}

bool readNumberField(const PlanJsonContext& context,
                     const Json& node,
                     const char* key,
                     const std::string& field_name,
                     double& value,
                     size_t command_index = 0,
                     bool has_command_index = false,
                     std::string_view command = {})
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidNumber,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a finite number.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "finite number",
                             "missing");
        return false;
    }

    return readNumberValue(context, *it, field_name, value, command_index, has_command_index, command);
}

bool readFloatField(const PlanJsonContext& context,
                    const Json& node,
                    const char* key,
                    const std::string& field_name,
                    float& value,
                    size_t command_index,
                    std::string_view command)
{
    double number = 0.0;
    if (!readNumberField(context, node, key, field_name, number, command_index, true, command)) {
        return false;
    }

    value = static_cast<float>(number);
    return true;
}

bool readVec3Value(const PlanJsonContext& context,
                   const Json& node,
                   const std::string& field_name,
                   glm::vec3& value,
                   size_t command_index,
                   std::string_view command)
{
    if (!node.is_array() || node.size() != 3) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a 3-number array.",
                             field_name,
                             command_index,
                             true,
                             std::string(command),
                             "3-number array");
        return false;
    }

    double values[3]{};
    for (size_t index = 0; index < 3; ++index) {
        if (!readNumberValue(context,
                             node[index],
                             field_name + "[" + std::to_string(index) + "]",
                             values[index],
                             command_index,
                             true,
                             command)) {
            return false;
        }
    }

    value = {static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2])};
    return true;
}

bool readVec3Field(const PlanJsonContext& context,
                   const Json& node,
                   const char* key,
                   const std::string& field_name,
                   glm::vec3& value,
                   size_t command_index,
                   std::string_view command)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a 3-number array.",
                             field_name,
                             command_index,
                             true,
                             std::string(command),
                             "3-number array",
                             "missing");
        return false;
    }

    return readVec3Value(context, *it, field_name, value, command_index, command);
}

bool readOptionalVec3Field(const PlanJsonContext& context,
                           const Json& node,
                           const char* key,
                           const std::string& field_name,
                           glm::vec3& value,
                           size_t command_index,
                           std::string_view command)
{
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;
    }

    return readVec3Value(context, *it, field_name, value, command_index, command);
}

bool readNonNegativeIntegerField(const PlanJsonContext& context,
                                 const Json& node,
                                 const char* key,
                                 const std::string& field_name,
                                 size_t& value,
                                 size_t command_index = 0,
                                 bool has_command_index = false,
                                 std::string_view command = {})
{
    double number = 0.0;
    if (!readNumberField(context, node, key, field_name, number, command_index, has_command_index, command)) {
        return false;
    }

    if (number < 0.0 || std::floor(number) != number) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidNumber,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field '" + field_name + "' must be a non-negative integer.",
                             field_name,
                             command_index,
                             has_command_index,
                             std::string(command),
                             "non-negative integer");
        return false;
    }

    value = static_cast<size_t>(number);
    return true;
}

bool parseCommand(const PlanJsonContext& context,
                  const Json& command_node,
                  size_t command_index,
                  AuthoringCommand& command)
{
    if (!command_node.is_object()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan command " + std::to_string(command_index) + " must be an object.",
                             "commands[" + std::to_string(command_index) + "]",
                             command_index,
                             true);
        return false;
    }

    std::string op;
    if (!readStringField(context,
                         command_node,
                         "op",
                         commandFieldName(command_index, "op"),
                         op,
                         command_index,
                         true)) {
        return false;
    }

    if (op == "new") {
        command.kind = AuthoringCommandKind::NewScene;
        return true;
    }

    if (op == "open") {
        command.kind = AuthoringCommandKind::OpenScene;
        std::string path;
        if (!readStringField(context,
                             command_node,
                             "path",
                             commandFieldName(command_index, "path"),
                             path,
                             command_index,
                             true,
                             op)) {
            return false;
        }
        command.path = std::move(path);
        return true;
    }

    if (op == "save") {
        command.kind = AuthoringCommandKind::SaveScene;
        std::string path;
        if (!readStringField(context,
                             command_node,
                             "path",
                             commandFieldName(command_index, "path"),
                             path,
                             command_index,
                             true,
                             op)) {
            return false;
        }
        command.path = std::move(path);
        return true;
    }

    if (op == "entity") {
        command.kind = AuthoringCommandKind::CreateEntity;
        return readStringField(context,
                               command_node,
                               "alias",
                               commandFieldName(command_index, "alias"),
                               command.alias,
                               command_index,
                               true,
                               op) &&
               readStringField(context,
                               command_node,
                               "name",
                               commandFieldName(command_index, "name"),
                               command.name,
                               command_index,
                               true,
                               op);
    }

    if (op == "camera") {
        command.kind = AuthoringCommandKind::CreateCamera;
        return readStringField(context,
                               command_node,
                               "alias",
                               commandFieldName(command_index, "alias"),
                               command.alias,
                               command_index,
                               true,
                               op);
    }

    if (op == "directional-light") {
        command.kind = AuthoringCommandKind::CreateDirectionalLight;
        return readStringField(context,
                               command_node,
                               "alias",
                               commandFieldName(command_index, "alias"),
                               command.alias,
                               command_index,
                               true,
                               op);
    }

    if (op == "point-light") {
        command.kind = AuthoringCommandKind::CreatePointLight;
        return readStringField(context,
                               command_node,
                               "alias",
                               commandFieldName(command_index, "alias"),
                               command.alias,
                               command_index,
                               true,
                               op);
    }

    if (op == "spot-light") {
        command.kind = AuthoringCommandKind::CreateSpotLight;
        return readStringField(context,
                               command_node,
                               "alias",
                               commandFieldName(command_index, "alias"),
                               command.alias,
                               command_index,
                               true,
                               op);
    }

    if (op == "primitive") {
        command.kind = AuthoringCommandKind::CreatePrimitive;
        return readStringField(context,
                               command_node,
                               "alias",
                               commandFieldName(command_index, "alias"),
                               command.alias,
                               command_index,
                               true,
                               op) &&
               readStringField(context,
                               command_node,
                               "mesh",
                               commandFieldName(command_index, "mesh"),
                               command.mesh,
                               command_index,
                               true,
                               op);
    }

    if (op == "parent") {
        command.kind = AuthoringCommandKind::Parent;
        return readStringField(context,
                               command_node,
                               "child",
                               commandFieldName(command_index, "child"),
                               command.child.value,
                               command_index,
                               true,
                               op) &&
               readStringField(context,
                               command_node,
                               "parent",
                               commandFieldName(command_index, "parent"),
                               command.parent.value,
                               command_index,
                               true,
                               op);
    }

    if (op == "unparent") {
        command.kind = AuthoringCommandKind::Unparent;
        return readStringField(context,
                               command_node,
                               "child",
                               commandFieldName(command_index, "child"),
                               command.child.value,
                               command_index,
                               true,
                               op);
    }

    if (op == "name") {
        command.kind = AuthoringCommandKind::Rename;
        return readStringField(context,
                               command_node,
                               "entity",
                               commandFieldName(command_index, "entity"),
                               command.entity.value,
                               command_index,
                               true,
                               op) &&
               readStringField(context,
                               command_node,
                               "name",
                               commandFieldName(command_index, "name"),
                               command.name,
                               command_index,
                               true,
                               op);
    }

    if (op == "transform") {
        command.kind = AuthoringCommandKind::SetTransform;
        return readStringField(context,
                               command_node,
                               "entity",
                               commandFieldName(command_index, "entity"),
                               command.entity.value,
                               command_index,
                               true,
                               op) &&
               readVec3Field(context,
                             command_node,
                             "translation",
                             commandFieldName(command_index, "translation"),
                             command.translation,
                             command_index,
                             op) &&
               readOptionalVec3Field(context,
                                     command_node,
                                     "rotationDeg",
                                     commandFieldName(command_index, "rotationDeg"),
                                     command.rotation_degrees,
                                     command_index,
                                     op) &&
               readOptionalVec3Field(context,
                                     command_node,
                                     "scale",
                                     commandFieldName(command_index, "scale"),
                                     command.scale,
                                     command_index,
                                     op);
    }

    if (op == "light-intensity") {
        command.kind = AuthoringCommandKind::SetLightIntensity;
        return readStringField(context,
                               command_node,
                               "entity",
                               commandFieldName(command_index, "entity"),
                               command.entity.value,
                               command_index,
                               true,
                               op) &&
               readFloatField(context,
                              command_node,
                              "value",
                              commandFieldName(command_index, "value"),
                              command.value,
                              command_index,
                              op);
    }

    if (op == "light-color") {
        command.kind = AuthoringCommandKind::SetLightColor;
        return readStringField(context,
                               command_node,
                               "entity",
                               commandFieldName(command_index, "entity"),
                               command.entity.value,
                               command_index,
                               true,
                               op) &&
               readVec3Field(context,
                             command_node,
                             "color",
                             commandFieldName(command_index, "color"),
                             command.color,
                             command_index,
                             op);
    }

    if (op == "camera-perspective") {
        command.kind = AuthoringCommandKind::SetCameraPerspective;
        return readStringField(context,
                               command_node,
                               "entity",
                               commandFieldName(command_index, "entity"),
                               command.entity.value,
                               command_index,
                               true,
                               op) &&
               readFloatField(context,
                              command_node,
                              "fovDeg",
                              commandFieldName(command_index, "fovDeg"),
                              command.fov_degrees,
                              command_index,
                              op) &&
               readFloatField(context,
                              command_node,
                              "near",
                              commandFieldName(command_index, "near"),
                              command.near_plane,
                              command_index,
                              op) &&
               readFloatField(context,
                              command_node,
                              "far",
                              commandFieldName(command_index, "far"),
                              command.far_plane,
                              command_index,
                              op);
    }

    if (op == "camera-orthographic") {
        command.kind = AuthoringCommandKind::SetCameraOrthographic;
        return readStringField(context,
                               command_node,
                               "entity",
                               commandFieldName(command_index, "entity"),
                               command.entity.value,
                               command_index,
                               true,
                               op) &&
               readFloatField(context,
                              command_node,
                              "size",
                              commandFieldName(command_index, "size"),
                              command.size,
                              command_index,
                              op) &&
               readFloatField(context,
                              command_node,
                              "near",
                              commandFieldName(command_index, "near"),
                              command.near_plane,
                              command_index,
                              op) &&
               readFloatField(context,
                              command_node,
                              "far",
                              commandFieldName(command_index, "far"),
                              command.far_plane,
                              command_index,
                              op);
    }

    if (op == "inspect") {
        std::string target;
        if (!readStringField(context,
                             command_node,
                             "target",
                             commandFieldName(command_index, "target"),
                             target,
                             command_index,
                             true,
                             op)) {
            return false;
        }

        if (target == "scene") {
            command.kind = AuthoringCommandKind::InspectScene;
            return true;
        }
        if (target == "hierarchy") {
            command.kind = AuthoringCommandKind::InspectHierarchy;
            return true;
        }
        if (target == "entity") {
            command.kind = AuthoringCommandKind::InspectEntity;
            return readStringField(context,
                                   command_node,
                                   "entity",
                                   commandFieldName(command_index, "entity"),
                                   command.entity.value,
                                   command_index,
                                   true,
                                   op);
        }

        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::UnsupportedCommand,
                             AuthoringDiagnosticPhase::Validate,
                             "Unsupported inspect target '" + target + "'.",
                             commandFieldName(command_index, "target"),
                             command_index,
                             true,
                             op,
                             "scene, hierarchy, or entity",
                             target);
        return false;
    }

    if (op == "verify") {
        std::string check;
        if (!readStringField(context,
                             command_node,
                             "check",
                             commandFieldName(command_index, "check"),
                             check,
                             command_index,
                             true,
                             op)) {
            return false;
        }

        if (check == "sceneSaved" || check == "saved") {
            command.kind = AuthoringCommandKind::VerifySceneSaved;
            return true;
        }
        if (check == "entityExists") {
            command.kind = AuthoringCommandKind::VerifyEntityExists;
            return readStringField(context,
                                   command_node,
                                   "entity",
                                   commandFieldName(command_index, "entity"),
                                   command.entity.value,
                                   command_index,
                                   true,
                                   op);
        }
        if (check == "hasComponent") {
            command.kind = AuthoringCommandKind::VerifyHasComponent;
            return readStringField(context,
                                   command_node,
                                   "entity",
                                   commandFieldName(command_index, "entity"),
                                   command.entity.value,
                                   command_index,
                                   true,
                                   op) &&
                   readStringField(context,
                                   command_node,
                                   "component",
                                   commandFieldName(command_index, "component"),
                                   command.component,
                                   command_index,
                                   true,
                                   op);
        }
        if (check == "entityCountAtLeast") {
            command.kind = AuthoringCommandKind::VerifyEntityCountAtLeast;
            return readNonNegativeIntegerField(context,
                                               command_node,
                                               "count",
                                               commandFieldName(command_index, "count"),
                                               command.count,
                                               command_index,
                                               true,
                                               op);
        }

        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::UnsupportedVerifyCheck,
                             AuthoringDiagnosticPhase::Validate,
                             "Unsupported verify check '" + check + "'.",
                             commandFieldName(command_index, "check"),
                             command_index,
                             true,
                             op,
                             "sceneSaved, entityExists, hasComponent, or entityCountAtLeast",
                             check);
        return false;
    }

    if (op == "summary") {
        command.kind = AuthoringCommandKind::Summary;
        return true;
    }

    appendPlanDiagnostic(context,
                         AuthoringDiagnosticCode::UnsupportedCommand,
                         AuthoringDiagnosticPhase::Validate,
                         "Unsupported plan op '" + op + "'.",
                         commandFieldName(command_index, "op"),
                         command_index,
                         true,
                         op);
    return false;
}

Json writeVec3(const glm::vec3& value)
{
    Json result = Json::array();
    result.push_back(value.x);
    result.push_back(value.y);
    result.push_back(value.z);
    return result;
}

Json writeCommand(const AuthoringCommand& command)
{
    Json result = Json::object();

    switch (command.kind) {
        case AuthoringCommandKind::NewScene:
            result["op"] = "new";
            return result;
        case AuthoringCommandKind::OpenScene:
            result["op"] = "open";
            result["path"] = command.path.string();
            return result;
        case AuthoringCommandKind::SaveScene:
            result["op"] = "save";
            result["path"] = command.path.string();
            return result;
        case AuthoringCommandKind::CreateEntity:
            result["op"] = "entity";
            result["alias"] = command.alias;
            result["name"] = command.name;
            return result;
        case AuthoringCommandKind::CreateCamera:
            result["op"] = "camera";
            result["alias"] = command.alias;
            return result;
        case AuthoringCommandKind::CreateDirectionalLight:
            result["op"] = "directional-light";
            result["alias"] = command.alias;
            return result;
        case AuthoringCommandKind::CreatePointLight:
            result["op"] = "point-light";
            result["alias"] = command.alias;
            return result;
        case AuthoringCommandKind::CreateSpotLight:
            result["op"] = "spot-light";
            result["alias"] = command.alias;
            return result;
        case AuthoringCommandKind::CreatePrimitive:
            result["op"] = "primitive";
            result["alias"] = command.alias;
            result["mesh"] = command.mesh;
            return result;
        case AuthoringCommandKind::Parent:
            result["op"] = "parent";
            result["child"] = command.child.value;
            result["parent"] = command.parent.value;
            return result;
        case AuthoringCommandKind::Unparent:
            result["op"] = "unparent";
            result["child"] = command.child.value;
            return result;
        case AuthoringCommandKind::Rename:
            result["op"] = "name";
            result["entity"] = command.entity.value;
            result["name"] = command.name;
            return result;
        case AuthoringCommandKind::SetTransform:
            result["op"] = "transform";
            result["entity"] = command.entity.value;
            result["translation"] = writeVec3(command.translation);
            result["rotationDeg"] = writeVec3(command.rotation_degrees);
            result["scale"] = writeVec3(command.scale);
            return result;
        case AuthoringCommandKind::SetLightIntensity:
            result["op"] = "light-intensity";
            result["entity"] = command.entity.value;
            result["value"] = command.value;
            return result;
        case AuthoringCommandKind::SetLightColor:
            result["op"] = "light-color";
            result["entity"] = command.entity.value;
            result["color"] = writeVec3(command.color);
            return result;
        case AuthoringCommandKind::SetCameraPerspective:
            result["op"] = "camera-perspective";
            result["entity"] = command.entity.value;
            result["fovDeg"] = command.fov_degrees;
            result["near"] = command.near_plane;
            result["far"] = command.far_plane;
            return result;
        case AuthoringCommandKind::SetCameraOrthographic:
            result["op"] = "camera-orthographic";
            result["entity"] = command.entity.value;
            result["size"] = command.size;
            result["near"] = command.near_plane;
            result["far"] = command.far_plane;
            return result;
        case AuthoringCommandKind::InspectScene:
            result["op"] = "inspect";
            result["target"] = "scene";
            return result;
        case AuthoringCommandKind::InspectEntity:
            result["op"] = "inspect";
            result["target"] = "entity";
            result["entity"] = command.entity.value;
            return result;
        case AuthoringCommandKind::InspectHierarchy:
            result["op"] = "inspect";
            result["target"] = "hierarchy";
            return result;
        case AuthoringCommandKind::VerifySceneSaved:
            result["op"] = "verify";
            result["check"] = "sceneSaved";
            return result;
        case AuthoringCommandKind::VerifyEntityExists:
            result["op"] = "verify";
            result["check"] = "entityExists";
            result["entity"] = command.entity.value;
            return result;
        case AuthoringCommandKind::VerifyHasComponent:
            result["op"] = "verify";
            result["check"] = "hasComponent";
            result["entity"] = command.entity.value;
            result["component"] = command.component;
            return result;
        case AuthoringCommandKind::VerifyEntityCountAtLeast:
            result["op"] = "verify";
            result["check"] = "entityCountAtLeast";
            result["count"] = command.count;
            return result;
        case AuthoringCommandKind::Summary:
            result["op"] = "summary";
            return result;
    }

    result["op"] = "summary";
    return result;
}

Json writePlan(const AuthoringPlan& plan)
{
    Json root = Json::object();

    Json protocol = Json::object();
    protocol["name"] = plan.protocol.name;
    protocol["version"] = plan.protocol.version;
    root["protocol"] = std::move(protocol);

    if (!plan.project_file_path.empty()) {
        root["project"] = plan.project_file_path.string();
    }

    Json commands = Json::array();
    for (const AuthoringCommand& command : plan.commands) {
        commands.push_back(writeCommand(command));
    }
    root["commands"] = std::move(commands);

    return root;
}

bool parsePlanJson(const Json& root,
                   AuthoringPlan& plan,
                   std::vector<std::string>& errors,
                   std::vector<AuthoringDiagnostic>* diagnostics,
                   const std::filesystem::path& source_path)
{
    PlanJsonContext context{errors, diagnostics, source_path};

    if (!root.is_object()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan root must be an object.");
        return false;
    }

    AuthoringPlan parsed_plan;

    if (const auto protocol = root.find("protocol"); protocol != root.end()) {
        if (!protocol->is_object()) {
            appendPlanDiagnostic(context,
                                 AuthoringDiagnosticCode::InvalidPlan,
                                 AuthoringDiagnosticPhase::Validate,
                                 "Plan field 'protocol' must be an object.",
                                 "protocol");
            return false;
        }

        if (!readStringField(context, *protocol, "name", "protocol.name", parsed_plan.protocol.name)) {
            return false;
        }
        if (parsed_plan.protocol.name != kAuthoringProtocolName) {
            appendPlanDiagnostic(context,
                                 AuthoringDiagnosticCode::ProtocolMismatch,
                                 AuthoringDiagnosticPhase::Validate,
                                 "Unsupported authoring protocol '" + parsed_plan.protocol.name + "'.",
                                 "protocol.name",
                                 0,
                                 false,
                                 {},
                                 std::string(kAuthoringProtocolName),
                                 parsed_plan.protocol.name);
            return false;
        }

        size_t protocol_version = 0;
        if (!readNonNegativeIntegerField(context, *protocol, "version", "protocol.version", protocol_version)) {
            return false;
        }
        if (protocol_version != kAuthoringProtocolVersion) {
            appendPlanDiagnostic(context,
                                 AuthoringDiagnosticCode::ProtocolMismatch,
                                 AuthoringDiagnosticPhase::Validate,
                                 "Unsupported authoring protocol version '" + std::to_string(protocol_version) + "'.",
                                 "protocol.version",
                                 0,
                                 false,
                                 {},
                                 std::to_string(kAuthoringProtocolVersion),
                                 std::to_string(protocol_version));
            return false;
        }
        parsed_plan.protocol.version = static_cast<uint32_t>(protocol_version);
    }

    if (const auto project = root.find("project"); project != root.end()) {
        std::string project_path;
        if (!readStringValue(context, *project, "project", project_path)) {
            return false;
        }
        parsed_plan.project_file_path = std::move(project_path);
    }

    const auto commands = root.find("commands");
    if (commands == root.end() || !commands->is_array()) {
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Validate,
                             "Plan field 'commands' must be an array.",
                             "commands");
        return false;
    }

    parsed_plan.commands.reserve(commands->size());
    for (size_t index = 0; index < commands->size(); ++index) {
        AuthoringCommand command;
        if (!parseCommand(context, (*commands)[index], index, command)) {
            return false;
        }
        parsed_plan.commands.push_back(std::move(command));
    }

    plan = std::move(parsed_plan);
    return true;
}

} // namespace

bool loadAuthoringPlanJson(std::istream& input,
                           AuthoringPlan& plan,
                           std::vector<std::string>& errors,
                           std::vector<AuthoringDiagnostic>* diagnostics,
                           const std::filesystem::path& source_path)
{
    Json root;
    try {
        input >> root;
    } catch (const std::exception& error) {
        PlanJsonContext context{errors, diagnostics, source_path};
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Parse,
                             source_path.empty()
                                 ? std::string("Failed to read plan: ") + error.what()
                                 : std::string("Failed to read plan '") + source_path.string() + "': " + error.what());
        return false;
    }

    return parsePlanJson(root, plan, errors, diagnostics, source_path);
}

bool loadAuthoringPlanJson(const std::filesystem::path& plan_path,
                           AuthoringPlan& plan,
                           std::vector<std::string>& errors,
                           std::vector<AuthoringDiagnostic>* diagnostics)
{
    std::ifstream input(plan_path, std::ios::binary);
    if (!input) {
        PlanJsonContext context{errors, diagnostics, plan_path};
        appendPlanDiagnostic(context,
                             AuthoringDiagnosticCode::InvalidPlan,
                             AuthoringDiagnosticPhase::Parse,
                             "Failed to read plan '" + plan_path.string() + "': unable to open file.");
        return false;
    }

    return loadAuthoringPlanJson(input, plan, errors, diagnostics, plan_path);
}

void writeAuthoringPlanJson(std::ostream& out, const AuthoringPlan& plan)
{
    out << writePlan(plan).dump(2, ' ', false, Json::error_handler_t::replace) << '\n';
}

} // namespace luna::authoring
