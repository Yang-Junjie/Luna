#include "AuthoringJson.h"

#include <glm/vec3.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace luna::authoring {
namespace {

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

void writeJsonVec3(std::ostream& out, const glm::vec3& value)
{
    out << "[" << value.x << ", " << value.y << ", " << value.z << "]";
}

void writeJsonUuid(std::ostream& out, UUID uuid)
{
    if (!uuid.isValid()) {
        out << "null";
        return;
    }

    out << "\"" << escapeAuthoringJsonString(uuid.toString()) << "\"";
}

void writeJsonStringArray(std::ostream& out, const std::vector<std::string>& values, size_t indent)
{
    const std::string padding(indent, ' ');
    out << "[\n";
    for (size_t index = 0; index < values.size(); ++index) {
        out << padding << "  \"" << escapeAuthoringJsonString(values[index]) << "\"";
        if (index + 1 < values.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << padding << "]";
}

void writeJsonUuidArray(std::ostream& out, const std::vector<UUID>& values, size_t indent)
{
    const std::string padding(indent, ' ');
    out << "[\n";
    for (size_t index = 0; index < values.size(); ++index) {
        out << padding << "  ";
        writeJsonUuid(out, values[index]);
        if (index + 1 < values.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << padding << "]";
}

void writeJsonAssetArray(std::ostream& out, const std::vector<AssetHandle>& values, size_t indent)
{
    const std::string padding(indent, ' ');
    out << "[\n";
    for (size_t index = 0; index < values.size(); ++index) {
        out << padding << "  ";
        writeJsonUuid(out, values[index]);
        if (index + 1 < values.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << padding << "]";
}

void writeJsonEntityInspection(std::ostream& out, const AuthoringEntityInspection& entity)
{
    out << "      {\n";
    out << "        \"ref\": \"" << escapeAuthoringJsonString(entity.ref) << "\",\n";
    out << "        \"uuid\": ";
    writeJsonUuid(out, entity.entity_id);
    out << ",\n";
    out << "        \"name\": \"" << escapeAuthoringJsonString(entity.name) << "\",\n";
    out << "        \"parentUuid\": ";
    writeJsonUuid(out, entity.parent_id);
    out << ",\n";
    out << "        \"children\": ";
    writeJsonUuidArray(out, entity.children, 8);
    out << ",\n";
    out << "        \"components\": ";
    writeJsonStringArray(out, entity.components, 8);

    if (entity.has_transform) {
        out << ",\n";
        out << "        \"transform\": {\n";
        out << "          \"translation\": ";
        writeJsonVec3(out, entity.transform.translation);
        out << ",\n";
        out << "          \"rotationDeg\": ";
        writeJsonVec3(out, entity.transform.rotation_degrees);
        out << ",\n";
        out << "          \"scale\": ";
        writeJsonVec3(out, entity.transform.scale);
        out << "\n";
        out << "        }";
    }

    if (entity.has_camera) {
        out << ",\n";
        out << "        \"camera\": {\n";
        out << "          \"primary\": " << (entity.camera.primary ? "true" : "false") << ",\n";
        out << "          \"fixedAspectRatio\": " << (entity.camera.fixed_aspect_ratio ? "true" : "false") << ",\n";
        out << "          \"projection\": \"" << escapeAuthoringJsonString(entity.camera.projection) << "\",\n";
        out << "          \"perspectiveFovDeg\": " << entity.camera.perspective_fov_degrees << ",\n";
        out << "          \"perspectiveNear\": " << entity.camera.perspective_near << ",\n";
        out << "          \"perspectiveFar\": " << entity.camera.perspective_far << ",\n";
        out << "          \"orthographicSize\": " << entity.camera.orthographic_size << ",\n";
        out << "          \"orthographicNear\": " << entity.camera.orthographic_near << ",\n";
        out << "          \"orthographicFar\": " << entity.camera.orthographic_far << "\n";
        out << "        }";
    }

    if (entity.has_light) {
        out << ",\n";
        out << "        \"light\": {\n";
        out << "          \"type\": \"" << escapeAuthoringJsonString(entity.light.type) << "\",\n";
        out << "          \"enabled\": " << (entity.light.enabled ? "true" : "false") << ",\n";
        out << "          \"color\": ";
        writeJsonVec3(out, entity.light.color);
        out << ",\n";
        out << "          \"intensity\": " << entity.light.intensity << ",\n";
        out << "          \"range\": " << entity.light.range << ",\n";
        out << "          \"innerConeAngleDeg\": " << entity.light.inner_cone_angle_degrees << ",\n";
        out << "          \"outerConeAngleDeg\": " << entity.light.outer_cone_angle_degrees << "\n";
        out << "        }";
    }

    if (entity.has_mesh) {
        out << ",\n";
        out << "        \"mesh\": {\n";
        out << "          \"meshHandle\": ";
        writeJsonUuid(out, entity.mesh.mesh_handle);
        out << ",\n";
        out << "          \"submeshMaterials\": ";
        writeJsonAssetArray(out, entity.mesh.submesh_materials, 10);
        out << "\n";
        out << "        }";
    }

    out << "\n";
    out << "      }";
}

void writeJsonInspections(std::ostream& out, const std::vector<AuthoringInspection>& inspections)
{
    out << "  \"inspections\": [\n";
    for (size_t inspection_index = 0; inspection_index < inspections.size(); ++inspection_index) {
        const AuthoringInspection& inspection = inspections[inspection_index];
        out << "    {\n";
        out << "      \"type\": \"" << inspectionKindName(inspection.kind) << "\",\n";
        out << "      \"ref\": \"" << escapeAuthoringJsonString(inspection.ref) << "\",\n";
        out << "      \"entities\": [\n";
        for (size_t entity_index = 0; entity_index < inspection.entities.size(); ++entity_index) {
            writeJsonEntityInspection(out, inspection.entities[entity_index]);
            if (entity_index + 1 < inspection.entities.size()) {
                out << ',';
            }
            out << '\n';
        }
        out << "      ]\n";
        out << "    }";
        if (inspection_index + 1 < inspections.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ],\n";
}

void writeJsonVerifications(std::ostream& out, const std::vector<AuthoringVerification>& verifications)
{
    out << "  \"verifications\": [\n";
    for (size_t index = 0; index < verifications.size(); ++index) {
        const AuthoringVerification& verification = verifications[index];
        out << "    {\n";
        out << "      \"type\": \"" << verificationKindName(verification.kind) << "\",\n";
        out << "      \"ok\": " << (verification.ok ? "true" : "false") << ",\n";
        out << "      \"ref\": \"" << escapeAuthoringJsonString(verification.ref) << "\",\n";
        out << "      \"uuid\": ";
        writeJsonUuid(out, verification.entity_id);
        out << ",\n";
        out << "      \"component\": \"" << escapeAuthoringJsonString(verification.component) << "\",\n";
        out << "      \"expectedCount\": " << verification.expected_count << ",\n";
        out << "      \"actualCount\": " << verification.actual_count << ",\n";
        out << "      \"message\": \"" << escapeAuthoringJsonString(verification.message) << "\"\n";
        out << "    }";
        if (index + 1 < verifications.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ],\n";
}

} // namespace

std::string escapeAuthoringJsonString(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (const char c : value) {
        switch (c) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += c;
                break;
        }
    }

    return escaped;
}

void writeAuthoringReportJson(std::ostream& out, const AuthoringReport& report, bool ok)
{
    out << "{\n";
    out << "  \"protocol\": {\n";
    out << "    \"name\": \"" << escapeAuthoringJsonString(report.protocol.name) << "\",\n";
    out << "    \"version\": " << report.protocol.version << "\n";
    out << "  },\n";
    out << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    out << "  \"scene\": {\n";
    out << "    \"name\": \"" << escapeAuthoringJsonString(report.scene.name) << "\",\n";
    out << "    \"path\": ";
    if (report.scene.path.empty()) {
        out << "null,\n";
    } else {
        out << "\"" << escapeAuthoringJsonString(report.scene.path.string()) << "\",\n";
    }
    out << "    \"entityCount\": " << report.scene.entity_count << ",\n";
    out << "    \"dirty\": " << (report.scene.dirty ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"entities\": [\n";
    for (size_t index = 0; index < report.entities.size(); ++index) {
        const AuthoringEntityBinding& binding = report.entities[index];
        out << "    {"
            << "\"alias\": \"" << escapeAuthoringJsonString(binding.alias) << "\", "
            << "\"uuid\": \"" << escapeAuthoringJsonString(binding.entity_id.toString()) << "\", "
            << "\"name\": \"" << escapeAuthoringJsonString(binding.name) << "\""
            << "}";
        if (index + 1 < report.entities.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ],\n";
    out << "  \"savedScenes\": [\n";
    for (size_t index = 0; index < report.saved_scenes.size(); ++index) {
        out << "    \"" << escapeAuthoringJsonString(report.saved_scenes[index].string()) << "\"";
        if (index + 1 < report.saved_scenes.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ],\n";
    writeJsonInspections(out, report.inspections);
    writeJsonVerifications(out, report.verifications);
    out << "  \"errors\": [\n";
    for (size_t index = 0; index < report.errors.size(); ++index) {
        out << "    \"" << escapeAuthoringJsonString(report.errors[index]) << "\"";
        if (index + 1 < report.errors.size()) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ]\n";
    out << "}\n";
}

} // namespace luna::authoring
