#include "AuthoringHostJson.h"
#include "AuthoringJson.h"

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace luna::authoring {
namespace {

using Json = nlohmann::ordered_json;

const char* inspectionKindName(AuthoringInspectionKind kind)
{
    switch (kind) {
        case AuthoringInspectionKind::Scene:
            return "scene";
        case AuthoringInspectionKind::Entity:
            return "entity";
        case AuthoringInspectionKind::Hierarchy:
            return "hierarchy";
    }

    return "unknown";
}

const char* verificationKindName(AuthoringVerificationKind kind)
{
    switch (kind) {
        case AuthoringVerificationKind::SceneSaved:
            return "sceneSaved";
        case AuthoringVerificationKind::EntityExists:
            return "entityExists";
        case AuthoringVerificationKind::HasComponent:
            return "hasComponent";
        case AuthoringVerificationKind::EntityCountAtLeast:
            return "entityCountAtLeast";
    }

    return "unknown";
}

const char* diagnosticSeverityName(AuthoringDiagnosticSeverity severity)
{
    switch (severity) {
        case AuthoringDiagnosticSeverity::Info:
            return "info";
        case AuthoringDiagnosticSeverity::Warning:
            return "warning";
        case AuthoringDiagnosticSeverity::Error:
            return "error";
    }

    return "unknown";
}

const char* diagnosticPhaseName(AuthoringDiagnosticPhase phase)
{
    switch (phase) {
        case AuthoringDiagnosticPhase::Parse:
            return "parse";
        case AuthoringDiagnosticPhase::Validate:
            return "validate";
        case AuthoringDiagnosticPhase::Execute:
            return "execute";
        case AuthoringDiagnosticPhase::Verify:
            return "verify";
    }

    return "unknown";
}

const char* diagnosticCodeName(AuthoringDiagnosticCode code)
{
    switch (code) {
        case AuthoringDiagnosticCode::InvalidPlan:
            return "InvalidPlan";
        case AuthoringDiagnosticCode::ProtocolMismatch:
            return "ProtocolMismatch";
        case AuthoringDiagnosticCode::UnsupportedCommand:
            return "UnsupportedCommand";
        case AuthoringDiagnosticCode::UnsupportedVerifyCheck:
            return "UnsupportedVerifyCheck";
        case AuthoringDiagnosticCode::MissingArgument:
            return "MissingArgument";
        case AuthoringDiagnosticCode::InvalidArgument:
            return "InvalidArgument";
        case AuthoringDiagnosticCode::InvalidNumber:
            return "InvalidNumber";
        case AuthoringDiagnosticCode::NoBoundScene:
            return "NoBoundScene";
        case AuthoringDiagnosticCode::UnknownEntity:
            return "UnknownEntity";
        case AuthoringDiagnosticCode::UnknownBuiltinAsset:
            return "UnknownBuiltinAsset";
        case AuthoringDiagnosticCode::MissingComponent:
            return "MissingComponent";
        case AuthoringDiagnosticCode::OpenSceneFailed:
            return "OpenSceneFailed";
        case AuthoringDiagnosticCode::SaveSceneFailed:
            return "SaveSceneFailed";
        case AuthoringDiagnosticCode::FileOverwrite:
            return "FileOverwrite";
        case AuthoringDiagnosticCode::ProjectLoadFailed:
            return "ProjectLoadFailed";
        case AuthoringDiagnosticCode::ExecutionFailed:
            return "ExecutionFailed";
        case AuthoringDiagnosticCode::VerificationFailed:
            return "VerificationFailed";
    }

    return "Unknown";
}

Json jsonVec3(const glm::vec3& value)
{
    Json vector = Json::array();
    vector.push_back(value.x);
    vector.push_back(value.y);
    vector.push_back(value.z);
    return vector;
}

Json jsonNullableString(std::string_view value)
{
    if (value.empty()) {
        return nullptr;
    }

    return std::string(value);
}

Json jsonNullablePath(const std::filesystem::path& value)
{
    if (value.empty()) {
        return nullptr;
    }

    return value.string();
}

Json jsonUuidOrNull(UUID uuid)
{
    if (!uuid.isValid()) {
        return nullptr;
    }

    return uuid.toString();
}

Json jsonStringArray(const std::vector<std::string>& values)
{
    Json result = Json::array();
    for (const std::string& value : values) {
        result.push_back(value);
    }
    return result;
}

Json jsonUuidArray(const std::vector<UUID>& values)
{
    Json result = Json::array();
    for (const UUID uuid : values) {
        result.push_back(jsonUuidOrNull(uuid));
    }
    return result;
}

Json jsonAssetArray(const std::vector<AssetHandle>& values)
{
    Json result = Json::array();
    for (const AssetHandle handle : values) {
        result.push_back(jsonUuidOrNull(handle));
    }
    return result;
}

Json jsonEntityInspection(const AuthoringEntityInspection& entity)
{
    Json result = Json::object();
    result["ref"] = entity.ref;
    result["uuid"] = jsonUuidOrNull(entity.entity_id);
    result["name"] = entity.name;
    result["parentUuid"] = jsonUuidOrNull(entity.parent_id);
    result["children"] = jsonUuidArray(entity.children);
    result["components"] = jsonStringArray(entity.components);

    if (entity.has_transform) {
        Json transform = Json::object();
        transform["translation"] = jsonVec3(entity.transform.translation);
        transform["rotationDeg"] = jsonVec3(entity.transform.rotation_degrees);
        transform["scale"] = jsonVec3(entity.transform.scale);
        result["transform"] = std::move(transform);
    }

    if (entity.has_camera) {
        Json camera = Json::object();
        camera["primary"] = entity.camera.primary;
        camera["fixedAspectRatio"] = entity.camera.fixed_aspect_ratio;
        camera["projection"] = entity.camera.projection;
        camera["perspectiveFovDeg"] = entity.camera.perspective_fov_degrees;
        camera["perspectiveNear"] = entity.camera.perspective_near;
        camera["perspectiveFar"] = entity.camera.perspective_far;
        camera["orthographicSize"] = entity.camera.orthographic_size;
        camera["orthographicNear"] = entity.camera.orthographic_near;
        camera["orthographicFar"] = entity.camera.orthographic_far;
        result["camera"] = std::move(camera);
    }

    if (entity.has_light) {
        Json light = Json::object();
        light["type"] = entity.light.type;
        light["enabled"] = entity.light.enabled;
        light["color"] = jsonVec3(entity.light.color);
        light["intensity"] = entity.light.intensity;
        light["range"] = entity.light.range;
        light["innerConeAngleDeg"] = entity.light.inner_cone_angle_degrees;
        light["outerConeAngleDeg"] = entity.light.outer_cone_angle_degrees;
        result["light"] = std::move(light);
    }

    if (entity.has_mesh) {
        Json mesh = Json::object();
        mesh["meshHandle"] = jsonUuidOrNull(entity.mesh.mesh_handle);
        if (entity.mesh.first_submesh != 0) {
            mesh["firstSubmesh"] = entity.mesh.first_submesh;
        }
        if (entity.mesh.submesh_count != UINT32_MAX) {
            mesh["submeshCount"] = entity.mesh.submesh_count;
        }
        mesh["submeshMaterials"] = jsonAssetArray(entity.mesh.submesh_materials);
        result["mesh"] = std::move(mesh);
    }

    return result;
}

Json jsonInspection(const AuthoringInspection& inspection)
{
    Json result = Json::object();
    result["type"] = inspectionKindName(inspection.kind);
    result["ref"] = inspection.ref;

    Json entities = Json::array();
    for (const AuthoringEntityInspection& entity : inspection.entities) {
        entities.push_back(jsonEntityInspection(entity));
    }
    result["entities"] = std::move(entities);
    return result;
}

Json jsonVerification(const AuthoringVerification& verification)
{
    Json result = Json::object();
    result["type"] = verificationKindName(verification.kind);
    result["ok"] = verification.ok;
    result["ref"] = verification.ref;
    result["uuid"] = jsonUuidOrNull(verification.entity_id);
    result["component"] = verification.component;
    result["expectedCount"] = verification.expected_count;
    result["actualCount"] = verification.actual_count;
    result["message"] = verification.message;
    return result;
}

Json jsonDiagnostic(const AuthoringDiagnostic& diagnostic)
{
    Json result = Json::object();
    result["severity"] = diagnosticSeverityName(diagnostic.severity);
    result["phase"] = diagnosticPhaseName(diagnostic.phase);
    result["code"] = diagnosticCodeName(diagnostic.code);
    result["message"] = diagnostic.message;
    result["commandIndex"] = diagnostic.has_command_index ? Json(diagnostic.command_index) : Json(nullptr);
    result["command"] = jsonNullableString(diagnostic.command);
    result["field"] = jsonNullableString(diagnostic.field);
    result["entityRef"] = jsonNullableString(diagnostic.entity_ref);
    result["component"] = jsonNullableString(diagnostic.component);
    result["path"] = jsonNullablePath(diagnostic.path);
    result["recoverable"] = diagnostic.recoverable;
    result["expected"] = jsonNullableString(diagnostic.expected);
    result["actual"] = jsonNullableString(diagnostic.actual);
    result["suggestedCommand"] = jsonNullableString(diagnostic.suggested_command);
    return result;
}

Json jsonEntityBindings(const std::vector<AuthoringEntityBinding>& bindings)
{
    Json result = Json::array();
    for (const AuthoringEntityBinding& binding : bindings) {
        Json entity = Json::object();
        entity["alias"] = binding.alias;
        entity["uuid"] = binding.entity_id.toString();
        entity["name"] = binding.name;
        result.push_back(std::move(entity));
    }
    return result;
}

Json jsonPathArray(const std::vector<std::filesystem::path>& paths)
{
    Json result = Json::array();
    for (const std::filesystem::path& path : paths) {
        result.push_back(path.string());
    }
    return result;
}

Json jsonInspections(const std::vector<AuthoringInspection>& inspections)
{
    Json result = Json::array();
    for (const AuthoringInspection& inspection : inspections) {
        result.push_back(jsonInspection(inspection));
    }
    return result;
}

Json jsonVerifications(const std::vector<AuthoringVerification>& verifications)
{
    Json result = Json::array();
    for (const AuthoringVerification& verification : verifications) {
        result.push_back(jsonVerification(verification));
    }
    return result;
}

Json jsonDiagnostics(const std::vector<AuthoringDiagnostic>& diagnostics)
{
    Json result = Json::array();
    for (const AuthoringDiagnostic& diagnostic : diagnostics) {
        result.push_back(jsonDiagnostic(diagnostic));
    }
    return result;
}

Json jsonErrors(const std::vector<std::string>& errors)
{
    Json result = Json::array();
    for (const std::string& error : errors) {
        result.push_back(error);
    }
    return result;
}

Json buildAuthoringReportJson(const AuthoringReport& report, bool ok)
{
    Json protocol = Json::object();
    protocol["name"] = report.protocol.name;
    protocol["version"] = report.protocol.version;

    Json result = Json::object();
    result["protocol"] = std::move(protocol);
    result["ok"] = ok;
    result["scene"] = authoringSceneSnapshotJson(report.scene);
    result["entities"] = jsonEntityBindings(report.entities);
    result["savedScenes"] = jsonPathArray(report.saved_scenes);
    result["inspections"] = jsonInspections(report.inspections);
    result["verifications"] = jsonVerifications(report.verifications);
    result["diagnostics"] = jsonDiagnostics(report.diagnostics);
    result["errors"] = jsonErrors(report.errors);
    return result;
}

} // namespace

Json authoringReportJson(const AuthoringReport& report, bool ok)
{
    return buildAuthoringReportJson(report, ok);
}

std::string escapeAuthoringJsonString(std::string_view value)
{
    const Json string_value = std::string(value);
    const std::string quoted = string_value.dump(-1, ' ', false, Json::error_handler_t::replace);
    if (quoted.size() < 2) {
        return quoted;
    }

    return quoted.substr(1, quoted.size() - 2);
}

void writeAuthoringReportJson(std::ostream& out, const AuthoringReport& report, bool ok)
{
    out << authoringReportJson(report, ok).dump(2, ' ', false, Json::error_handler_t::replace) << '\n';
}

} // namespace luna::authoring
