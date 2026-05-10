#include "Authoring/AuthoringCapabilities.h"
#include "Authoring/AuthoringHostJson.h"
#include "Authoring/AuthoringJson.h"
#include "Authoring/AuthoringPlanJson.h"
#include "Authoring/AuthoringProtocol.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

std::vector<luna::authoring::AuthoringCommandKind> allAuthoringCommandKinds()
{
    using luna::authoring::AuthoringCommandKind;

    return {
        AuthoringCommandKind::NewScene,
        AuthoringCommandKind::OpenScene,
        AuthoringCommandKind::SaveScene,
        AuthoringCommandKind::CreateEntity,
        AuthoringCommandKind::CreateCamera,
        AuthoringCommandKind::CreateDirectionalLight,
        AuthoringCommandKind::CreatePointLight,
        AuthoringCommandKind::CreateSpotLight,
        AuthoringCommandKind::CreatePrimitive,
        AuthoringCommandKind::Parent,
        AuthoringCommandKind::Unparent,
        AuthoringCommandKind::Rename,
        AuthoringCommandKind::SetTransform,
        AuthoringCommandKind::SetLightIntensity,
        AuthoringCommandKind::SetLightColor,
        AuthoringCommandKind::SetCameraPerspective,
        AuthoringCommandKind::SetCameraOrthographic,
        AuthoringCommandKind::InspectScene,
        AuthoringCommandKind::InspectEntity,
        AuthoringCommandKind::InspectHierarchy,
        AuthoringCommandKind::VerifySceneSaved,
        AuthoringCommandKind::VerifyEntityExists,
        AuthoringCommandKind::VerifyHasComponent,
        AuthoringCommandKind::VerifyEntityCountAtLeast,
        AuthoringCommandKind::Snapshot,
        AuthoringCommandKind::Summary,
    };
}

void testCommandTokenParsing(TestContext& context)
{
    const std::vector<std::string> tokens{
        "new",
        "primitive",
        "Box",
        "Cube",
        "inspect",
        "entity",
        "Box",
        "verify",
        "component",
        "Box",
        "Mesh",
        "verify",
        "entity-count-at-least",
        "3",
        "snapshot",
    };

    luna::authoring::AuthoringPlan plan;
    std::vector<std::string> errors;
    context.expect(luna::authoring::parseAuthoringCommandTokens(tokens, plan, errors),
                   "valid authoring tokens should parse");
    context.expect(errors.empty(), "valid authoring tokens should not report parse errors");
    context.expect(plan.commands.size() == 6, "valid authoring tokens should produce six commands");
    context.expect(plan.commands[0].kind == luna::authoring::AuthoringCommandKind::NewScene,
                   "first command should be new scene");
    context.expect(plan.commands[1].kind == luna::authoring::AuthoringCommandKind::CreatePrimitive,
                   "second command should be create primitive");
    context.expect(plan.commands[1].alias == "Box" && plan.commands[1].mesh == "Cube",
                   "primitive command should preserve alias and mesh");
    context.expect(plan.commands[2].kind == luna::authoring::AuthoringCommandKind::InspectEntity,
                   "third command should be inspect entity");
    context.expect(plan.commands[3].kind == luna::authoring::AuthoringCommandKind::VerifyHasComponent,
                   "fourth command should be verify component");
    context.expect(plan.commands[3].entity.value == "Box" && plan.commands[3].component == "Mesh",
                   "verify component should preserve entity and component");
    context.expect(plan.commands[4].kind == luna::authoring::AuthoringCommandKind::VerifyEntityCountAtLeast,
                   "fifth command should be verify entity count");
    context.expect(plan.commands[4].count == 3, "verify entity count should parse count");
    context.expect(plan.commands[5].kind == luna::authoring::AuthoringCommandKind::Snapshot,
                   "sixth command should be snapshot");
}

void testCommandTokenParseErrors(TestContext& context)
{
    const std::vector<std::string> tokens{"verify", "entity-count-at-least", "not-a-number"};

    luna::authoring::AuthoringPlan plan;
    std::vector<std::string> errors;
    std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
    context.expect(!luna::authoring::parseAuthoringCommandTokens(tokens, plan, errors, 0, &diagnostics),
                   "invalid authoring tokens should fail");
    context.expect(!errors.empty(), "invalid authoring tokens should report parse errors");
    context.expect(diagnostics.size() == 1, "invalid authoring tokens should report one diagnostic");
    if (!diagnostics.empty()) {
        context.expect(diagnostics.front().code == luna::authoring::AuthoringDiagnosticCode::InvalidNumber,
                       "invalid entity count should use InvalidNumber diagnostic code");
        context.expect(diagnostics.front().phase == luna::authoring::AuthoringDiagnosticPhase::Parse,
                       "token parse diagnostic should use parse phase");
        context.expect(diagnostics.front().has_command_index && diagnostics.front().command_index == 0,
                       "token parse diagnostic should include command index");
        context.expect(diagnostics.front().command == "verify",
                       "token parse diagnostic should include command name");
        context.expect(diagnostics.front().recoverable, "token parse diagnostic should be recoverable");
    }
}

void testCommandEffectClassification(TestContext& context)
{
    using luna::authoring::AuthoringCommandKind;

    size_t file_read_count = 0;
    size_t file_write_count = 0;
    for (const AuthoringCommandKind kind : allAuthoringCommandKinds()) {
        if (luna::authoring::authoringCommandReadsFileSystem(kind)) {
            ++file_read_count;
        }
        if (luna::authoring::authoringCommandWritesFileSystem(kind)) {
            ++file_write_count;
        }
    }

    context.expect(file_read_count == 1, "only open should currently read the filesystem");
    context.expect(file_write_count == 1, "only save should currently write the filesystem");
    context.expect(luna::authoring::authoringCommandReadsFileSystem(AuthoringCommandKind::OpenScene),
                   "open should declare filesystem read");
    context.expect(luna::authoring::authoringCommandMutatesScene(AuthoringCommandKind::OpenScene),
                   "open should declare scene mutation");
    context.expect(!luna::authoring::authoringCommandWritesFileSystem(AuthoringCommandKind::OpenScene),
                   "open should not declare filesystem write");
    context.expect(luna::authoring::authoringCommandWritesFileSystem(AuthoringCommandKind::SaveScene),
                   "save should declare filesystem write");
    context.expect(luna::authoring::authoringCommandMutatesScene(AuthoringCommandKind::SaveScene),
                   "save should declare scene mutation");
    context.expect(!luna::authoring::authoringCommandIsReadOnly(AuthoringCommandKind::SaveScene),
                   "save should not be read-only");
    context.expect(luna::authoring::authoringCommandIsReadOnly(AuthoringCommandKind::InspectScene),
                   "inspect scene should be read-only");
    context.expect(luna::authoring::authoringCommandReadsScene(AuthoringCommandKind::InspectEntity),
                   "inspect entity should declare scene read");
    context.expect(luna::authoring::authoringCommandIsReadOnly(AuthoringCommandKind::VerifyEntityExists),
                   "verify entity should be read-only");
    context.expect(luna::authoring::authoringCommandReadsScene(AuthoringCommandKind::Snapshot),
                   "snapshot should declare scene read");
    context.expect(luna::authoring::authoringCommandIsReadOnly(AuthoringCommandKind::Snapshot),
                   "snapshot should be read-only");
    context.expect(luna::authoring::authoringCommandIsReadOnly(AuthoringCommandKind::Summary),
                   "summary should be read-only");
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

std::filesystem::path authoringFixturePath(std::string_view filename)
{
    return std::filesystem::path{LUNA_AUTHORING_FIXTURE_DIR} / std::string(filename);
}

std::filesystem::path authoringSchemaPath(std::string_view filename)
{
    return std::filesystem::path{LUNA_AUTHORING_SCHEMA_DIR} / std::string(filename);
}

std::string normalizeNewlines(std::string text)
{
    std::string normalized;
    normalized.reserve(text.size());

    for (size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\r') {
            if (index + 1 < text.size() && text[index + 1] == '\n') {
                ++index;
            }
            normalized.push_back('\n');
        } else {
            normalized.push_back(text[index]);
        }
    }

    return normalized;
}

bool readTextFile(const std::filesystem::path& path, std::string& text)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

void expectGoldenText(TestContext& context,
                      std::string_view actual,
                      const std::filesystem::path& golden_path,
                      std::string_view message)
{
    std::string expected;
    if (!context.expect(readTextFile(golden_path, expected), "golden snapshot should be readable")) {
        return;
    }

    context.expect(normalizeNewlines(std::string(actual)) == normalizeNewlines(expected), message);
}

std::set<std::string> uniqueAuthoringCommandNames()
{
    std::set<std::string> names;
    for (const luna::authoring::AuthoringCommandKind kind : allAuthoringCommandKinds()) {
        names.insert(luna::authoring::authoringCommandName(kind));
    }
    return names;
}

std::set<std::string> schemaCommandOps(const YAML::Node& plan_schema)
{
    std::set<std::string> ops;
    const YAML::Node refs = plan_schema["$defs"]["command"]["oneOf"];
    if (!refs.IsSequence()) {
        return ops;
    }

    for (const YAML::Node ref : refs) {
        const std::string ref_path = ref["$ref"].as<std::string>();
        const size_t name_begin = ref_path.rfind('/');
        if (name_begin == std::string::npos || name_begin + 1 >= ref_path.size()) {
            continue;
        }

        const std::string def_name = ref_path.substr(name_begin + 1);
        const YAML::Node command_def = plan_schema["$defs"][def_name];
        const YAML::Node op = command_def["properties"]["op"]["const"];
        if (op.IsScalar()) {
            ops.insert(op.as<std::string>());
        }
    }

    return ops;
}

std::set<std::string> capabilityOps(const std::vector<luna::authoring::AuthoringCapability>& capabilities)
{
    std::set<std::string> ops;
    for (const luna::authoring::AuthoringCapability& capability : capabilities) {
        ops.insert(capability.op);
    }
    return ops;
}

std::set<std::string> schemaHostWireMethodNames(const YAML::Node& wire_schema)
{
    std::set<std::string> names;
    const YAML::Node method_names = wire_schema["$defs"]["method"]["properties"]["name"]["enum"];
    if (!method_names.IsSequence()) {
        return names;
    }

    for (const YAML::Node method_name : method_names) {
        if (method_name.IsScalar()) {
            names.insert(method_name.as<std::string>());
        }
    }
    return names;
}

std::set<std::string> manifestHostWireMethodNames(const luna::authoring::Json& manifest)
{
    std::set<std::string> names;
    if (!manifest.contains("methods") || !manifest["methods"].is_array()) {
        return names;
    }

    for (const auto& method : manifest["methods"]) {
        if (method.contains("name") && method["name"].is_string()) {
            names.insert(method["name"].get<std::string>());
        }
    }
    return names;
}

void testAuthoringPlanJsonRoundTrip(TestContext& context)
{
    using luna::authoring::AuthoringCommandKind;

    luna::authoring::AuthoringPlan plan;
    plan.project_file_path = "Projects/Example.lunaproj";
    plan.commands.push_back({.kind = AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = AuthoringCommandKind::OpenScene,
        .path = "Scenes/Existing.lunascene",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::SaveScene,
        .path = "Scenes/Saved.lunascene",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::CreateEntity,
        .alias = "Root",
        .name = "Root Entity",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::CreateCamera,
        .alias = "MainCamera",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::CreateDirectionalLight,
        .alias = "Sun",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::CreatePointLight,
        .alias = "Lamp",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::CreateSpotLight,
        .alias = "Spot",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::CreatePrimitive,
        .alias = "Box",
        .mesh = "Cube",
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::Parent,
        .child = {.value = "Box"},
        .parent = {.value = "Root"},
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::Unparent,
        .child = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::Rename,
        .name = "Renamed Box",
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::SetTransform,
        .entity = {.value = "Box"},
        .translation = {1.0f, 2.0f, 3.0f},
        .rotation_degrees = {10.0f, 20.0f, 30.0f},
        .scale = {1.5f, 2.0f, 0.5f},
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::SetLightIntensity,
        .entity = {.value = "Sun"},
        .value = 4.5f,
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::SetLightColor,
        .entity = {.value = "Sun"},
        .color = {0.5f, 0.25f, 0.75f},
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::SetCameraPerspective,
        .entity = {.value = "MainCamera"},
        .fov_degrees = 60.0f,
        .near_plane = 0.125f,
        .far_plane = 1024.0f,
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::SetCameraOrthographic,
        .entity = {.value = "MainCamera"},
        .near_plane = 0.25f,
        .far_plane = 512.0f,
        .size = 12.0f,
    });
    plan.commands.push_back({.kind = AuthoringCommandKind::InspectScene});
    plan.commands.push_back({
        .kind = AuthoringCommandKind::InspectEntity,
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({.kind = AuthoringCommandKind::InspectHierarchy});
    plan.commands.push_back({.kind = AuthoringCommandKind::VerifySceneSaved});
    plan.commands.push_back({
        .kind = AuthoringCommandKind::VerifyEntityExists,
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::VerifyHasComponent,
        .component = "Mesh",
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = AuthoringCommandKind::VerifyEntityCountAtLeast,
        .count = 5,
    });
    plan.commands.push_back({.kind = AuthoringCommandKind::Snapshot});
    plan.commands.push_back({.kind = AuthoringCommandKind::Summary});

    std::ostringstream stream;
    luna::authoring::writeAuthoringPlanJson(stream, plan);

    try {
        const YAML::Node root = YAML::Load(stream.str());
        context.expect(root["commands"][20]["check"].as<std::string>() == "sceneSaved",
                       "scene saved verification should serialize as sceneSaved");
    } catch (const YAML::Exception& error) {
        context.expect(false, std::string("plan JSON should parse for serialized checks: ") + error.what());
    }

    luna::authoring::AuthoringPlan parsed_plan;
    std::vector<std::string> errors;
    std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
    std::istringstream input(stream.str());
    context.expect(luna::authoring::loadAuthoringPlanJson(input, parsed_plan, errors, &diagnostics),
                   "authoring plan JSON should round-trip");
    context.expect(errors.empty(), "authoring plan JSON round-trip should not report errors");
    context.expect(diagnostics.empty(), "authoring plan JSON round-trip should not report diagnostics");
    context.expect(parsed_plan.protocol.name == std::string(luna::authoring::kAuthoringProtocolName),
                   "authoring plan JSON should preserve protocol name");
    context.expect(parsed_plan.protocol.version == luna::authoring::kAuthoringProtocolVersion,
                   "authoring plan JSON should preserve protocol version");
    context.expect(parsed_plan.project_file_path == plan.project_file_path,
                   "authoring plan JSON should preserve project path");
    context.expect(parsed_plan.commands.size() == plan.commands.size(),
                   "authoring plan JSON should preserve command count");

    if (parsed_plan.commands.size() != plan.commands.size()) {
        return;
    }

    for (size_t index = 0; index < plan.commands.size(); ++index) {
        context.expect(parsed_plan.commands[index].kind == plan.commands[index].kind,
                       "authoring plan JSON should preserve command kind");
    }

    context.expect(parsed_plan.commands[1].path == "Scenes/Existing.lunascene",
                   "authoring plan JSON should preserve open path");
    context.expect(parsed_plan.commands[2].path == "Scenes/Saved.lunascene",
                   "authoring plan JSON should preserve save path");
    context.expect(parsed_plan.commands[3].alias == "Root" && parsed_plan.commands[3].name == "Root Entity",
                   "authoring plan JSON should preserve entity command");
    context.expect(parsed_plan.commands[4].alias == "MainCamera",
                   "authoring plan JSON should preserve camera alias");
    context.expect(parsed_plan.commands[5].alias == "Sun",
                   "authoring plan JSON should preserve directional light alias");
    context.expect(parsed_plan.commands[6].alias == "Lamp",
                   "authoring plan JSON should preserve point light alias");
    context.expect(parsed_plan.commands[7].alias == "Spot",
                   "authoring plan JSON should preserve spot light alias");
    context.expect(parsed_plan.commands[8].alias == "Box" && parsed_plan.commands[8].mesh == "Cube",
                   "authoring plan JSON should preserve primitive command");
    context.expect(parsed_plan.commands[9].child.value == "Box" && parsed_plan.commands[9].parent.value == "Root",
                   "authoring plan JSON should preserve parent command");
    context.expect(parsed_plan.commands[10].child.value == "Box",
                   "authoring plan JSON should preserve unparent command");
    context.expect(parsed_plan.commands[11].entity.value == "Box" && parsed_plan.commands[11].name == "Renamed Box",
                   "authoring plan JSON should preserve rename command");
    context.expect(parsed_plan.commands[12].entity.value == "Box" &&
                       sameVec3(parsed_plan.commands[12].translation, {1.0f, 2.0f, 3.0f}) &&
                       sameVec3(parsed_plan.commands[12].rotation_degrees, {10.0f, 20.0f, 30.0f}) &&
                       sameVec3(parsed_plan.commands[12].scale, {1.5f, 2.0f, 0.5f}),
                   "authoring plan JSON should preserve transform command");
    context.expect(parsed_plan.commands[13].entity.value == "Sun" && parsed_plan.commands[13].value == 4.5f,
                   "authoring plan JSON should preserve light intensity command");
    context.expect(parsed_plan.commands[14].entity.value == "Sun" &&
                       sameVec3(parsed_plan.commands[14].color, {0.5f, 0.25f, 0.75f}),
                   "authoring plan JSON should preserve light color command");
    context.expect(parsed_plan.commands[15].entity.value == "MainCamera" &&
                       parsed_plan.commands[15].fov_degrees == 60.0f &&
                       parsed_plan.commands[15].near_plane == 0.125f &&
                       parsed_plan.commands[15].far_plane == 1024.0f,
                   "authoring plan JSON should preserve camera perspective command");
    context.expect(parsed_plan.commands[16].entity.value == "MainCamera" &&
                       parsed_plan.commands[16].size == 12.0f &&
                       parsed_plan.commands[16].near_plane == 0.25f &&
                       parsed_plan.commands[16].far_plane == 512.0f,
                   "authoring plan JSON should preserve camera orthographic command");
    context.expect(parsed_plan.commands[18].entity.value == "Box",
                   "authoring plan JSON should preserve inspect entity command");
    context.expect(parsed_plan.commands[21].entity.value == "Box",
                   "authoring plan JSON should preserve verify entity command");
    context.expect(parsed_plan.commands[22].entity.value == "Box" && parsed_plan.commands[22].component == "Mesh",
                   "authoring plan JSON should preserve verify component command");
    context.expect(parsed_plan.commands[23].count == 5,
                   "authoring plan JSON should preserve verify count command");
    context.expect(parsed_plan.commands[24].kind == AuthoringCommandKind::Snapshot,
                   "authoring plan JSON should preserve snapshot command");
}

void testAuthoringPlanJsonAcceptsMissingProtocol(TestContext& context)
{
    const std::string plan_text = R"({
  "commands": [
    { "op": "new" }
  ]
})";

    luna::authoring::AuthoringPlan plan;
    std::vector<std::string> errors;
    std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
    std::istringstream input(plan_text);
    context.expect(luna::authoring::loadAuthoringPlanJson(input, plan, errors, &diagnostics),
                   "authoring plan JSON should allow missing protocol");
    context.expect(errors.empty(), "missing protocol plan should not report errors");
    context.expect(diagnostics.empty(), "missing protocol plan should not report diagnostics");
    context.expect(plan.protocol.name == std::string(luna::authoring::kAuthoringProtocolName),
                   "missing protocol plan should keep default protocol name");
    context.expect(plan.protocol.version == luna::authoring::kAuthoringProtocolVersion,
                   "missing protocol plan should keep default protocol version");
    context.expect(plan.commands.size() == 1 &&
                       plan.commands.front().kind == luna::authoring::AuthoringCommandKind::NewScene,
                   "missing protocol plan should parse commands");
}

void testAuthoringPlanJsonReportsProtocolMismatch(TestContext& context)
{
    const std::string plan_text = R"({
  "protocol": { "name": "other.authoring", "version": 1 },
  "commands": [
    { "op": "new" }
  ]
})";

    luna::authoring::AuthoringPlan plan;
    std::vector<std::string> errors;
    std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
    std::istringstream input(plan_text);
    context.expect(!luna::authoring::loadAuthoringPlanJson(input, plan, errors, &diagnostics),
                   "mismatched protocol plan should fail");
    context.expect(!errors.empty(), "mismatched protocol plan should report errors");
    context.expect(diagnostics.size() == 1, "mismatched protocol plan should report one diagnostic");
    if (!diagnostics.empty()) {
        context.expect(diagnostics.front().code == luna::authoring::AuthoringDiagnosticCode::ProtocolMismatch,
                       "mismatched protocol plan should use ProtocolMismatch");
        context.expect(diagnostics.front().phase == luna::authoring::AuthoringDiagnosticPhase::Validate,
                       "mismatched protocol plan should validate protocol");
        context.expect(diagnostics.front().field == "protocol.name",
                       "mismatched protocol plan should identify the protocol field");
    }
}

void testAuthoringPlanJsonRejectsUnknownFields(TestContext& context)
{
    struct Case {
        const char* plan_text;
        const char* expected_field;
        bool has_command_index;
        size_t command_index;
        const char* command;
    };

    const Case cases[]{
        {
            R"({ "extra": true, "commands": [] })",
            "extra",
            false,
            0,
            "",
        },
        {
            R"({
  "protocol": { "name": "luna.authoring", "version": 1, "extra": true },
  "commands": []
})",
            "protocol.extra",
            false,
            0,
            "",
        },
        {
            R"({
  "commands": [
    { "op": "primitive", "alias": "Box", "mesh": "Cube", "material": "Default" }
  ]
})",
            "commands[0].material",
            true,
            0,
            "primitive",
        },
    };

    for (const Case& test_case : cases) {
        luna::authoring::AuthoringPlan plan;
        std::vector<std::string> errors;
        std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
        std::istringstream input{test_case.plan_text};
        context.expect(!luna::authoring::loadAuthoringPlanJson(input, plan, errors, &diagnostics),
                       "authoring plan JSON should reject unknown fields");
        context.expect(!errors.empty(), "unknown fields should report errors");
        context.expect(diagnostics.size() == 1, "unknown fields should report one diagnostic");
        if (!diagnostics.empty()) {
            context.expect(diagnostics.front().code == luna::authoring::AuthoringDiagnosticCode::InvalidPlan,
                           "unknown fields should use InvalidPlan diagnostic code");
            context.expect(diagnostics.front().phase == luna::authoring::AuthoringDiagnosticPhase::Validate,
                           "unknown fields should use validate phase");
            context.expect(diagnostics.front().field == test_case.expected_field,
                           "unknown fields should report stable field path");
            context.expect(diagnostics.front().has_command_index == test_case.has_command_index,
                           "unknown fields should report command index only for command fields");
            if (test_case.has_command_index) {
                context.expect(diagnostics.front().command_index == test_case.command_index,
                               "unknown command fields should report command index");
                context.expect(diagnostics.front().command == test_case.command,
                               "unknown command fields should report command name");
            }
            context.expect(diagnostics.front().recoverable, "unknown fields should be recoverable");
        }
    }
}

void testAuthoringPlanJsonAcceptsLegacySavedCheck(TestContext& context)
{
    const std::string plan_text = R"({
  "commands": [
    { "op": "verify", "check": "saved" }
  ]
})";

    luna::authoring::AuthoringPlan plan;
    std::vector<std::string> errors;
    std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
    std::istringstream input(plan_text);
    context.expect(luna::authoring::loadAuthoringPlanJson(input, plan, errors, &diagnostics),
                   "authoring plan JSON should accept legacy saved verify check");
    context.expect(errors.empty(), "legacy saved verify check should not report errors");
    context.expect(diagnostics.empty(), "legacy saved verify check should not report diagnostics");
    context.expect(plan.commands.size() == 1 &&
                       plan.commands.front().kind == luna::authoring::AuthoringCommandKind::VerifySceneSaved,
                   "legacy saved verify check should parse as VerifySceneSaved");
}

void expectAuthoringPlanJsonFailure(TestContext& context,
                                    std::string_view plan_text,
                                    luna::authoring::AuthoringDiagnosticCode expected_code,
                                    luna::authoring::AuthoringDiagnosticPhase expected_phase,
                                    std::string_view expected_field,
                                    std::string_view message)
{
    luna::authoring::AuthoringPlan plan;
    std::vector<std::string> errors;
    std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
    std::istringstream input{std::string(plan_text)};
    context.expect(!luna::authoring::loadAuthoringPlanJson(input, plan, errors, &diagnostics), message);
    context.expect(!errors.empty(), "invalid authoring plan JSON should report errors");
    context.expect(diagnostics.size() == 1, "invalid authoring plan JSON should report one diagnostic");
    if (!diagnostics.empty()) {
        context.expect(diagnostics.front().code == expected_code,
                       "invalid authoring plan JSON should report stable diagnostic code");
        context.expect(diagnostics.front().phase == expected_phase,
                       "invalid authoring plan JSON should report stable diagnostic phase");
        context.expect(diagnostics.front().field == expected_field,
                       "invalid authoring plan JSON should report stable field");
        context.expect(diagnostics.front().has_command_index && diagnostics.front().command_index == 0,
                       "invalid authoring plan JSON should include command index");
        context.expect(diagnostics.front().recoverable,
                       "invalid authoring plan JSON diagnostic should be recoverable");
    }
}

void testAuthoringPlanJsonParseDiagnostics(TestContext& context)
{
    expectAuthoringPlanJsonFailure(context,
                                   R"({ "commands": [ { "op": "spawn" } ] })",
                                   luna::authoring::AuthoringDiagnosticCode::UnsupportedCommand,
                                   luna::authoring::AuthoringDiagnosticPhase::Validate,
                                   "commands[0].op",
                                   "unsupported authoring plan op should fail");

    expectAuthoringPlanJsonFailure(context,
                                   R"({ "commands": [ { "op": "transform", "entity": "Box" } ] })",
                                   luna::authoring::AuthoringDiagnosticCode::InvalidPlan,
                                   luna::authoring::AuthoringDiagnosticPhase::Validate,
                                   "commands[0].translation",
                                   "missing authoring plan vec3 field should fail");

    expectAuthoringPlanJsonFailure(context,
                                   R"({ "commands": [ { "op": "light-intensity", "entity": "Sun", "value": "bright" } ] })",
                                   luna::authoring::AuthoringDiagnosticCode::InvalidNumber,
                                   luna::authoring::AuthoringDiagnosticPhase::Validate,
                                   "commands[0].value",
                                   "invalid authoring plan number should fail");

    expectAuthoringPlanJsonFailure(context,
                                   R"({ "commands": [ { "op": "verify", "check": "selected" } ] })",
                                   luna::authoring::AuthoringDiagnosticCode::UnsupportedVerifyCheck,
                                   luna::authoring::AuthoringDiagnosticPhase::Validate,
                                   "commands[0].check",
                                   "unsupported authoring plan verify check should fail");
}

void testAuthoringJsonSchemasParse(TestContext& context)
{
    for (const char* filename : {
             "authoring-plan.schema.json",
             "authoring-report.schema.json",
             "authoring-capabilities.schema.json",
             "authoring-host-wire.schema.json",
         }) {
        const std::filesystem::path path = authoringSchemaPath(filename);
        try {
            const YAML::Node root = YAML::LoadFile(path.string());
            context.expect(root.IsMap(), "authoring JSON schema should parse as an object");
            context.expect(root["$schema"].as<std::string>() == "https://json-schema.org/draft/2020-12/schema",
                           "authoring JSON schema should declare draft 2020-12");
            context.expect(root["$id"].as<std::string>().find("https://luna.local/schemas/") == 0,
                           "authoring JSON schema should declare a stable schema id");
            context.expect(root["$defs"].IsMap(), "authoring JSON schema should declare reusable definitions");
        } catch (const YAML::Exception& error) {
            context.expect(false, std::string("authoring JSON schema should parse: ") + error.what());
        }
    }
}

void testAuthoringCapabilities(TestContext& context)
{
    const std::vector<luna::authoring::AuthoringCapability> capabilities =
        luna::authoring::defaultAuthoringCapabilities();
    context.expect(capabilities.size() == 21, "default authoring capabilities should cover current ops");

    bool has_primitive = false;
    bool has_save = false;
    bool has_verify = false;
    bool has_snapshot = false;
    for (const luna::authoring::AuthoringCapability& capability : capabilities) {
        if (capability.op == "primitive") {
            has_primitive = true;
            context.expect(luna::authoring::hasAuthoringCommandEffect(capability.effects,
                                                                      luna::authoring::AuthoringCommandEffect::MutatesScene),
                           "primitive capability should declare scene mutation");
            bool has_mesh_enum = false;
            for (const luna::authoring::AuthoringCapabilityParameter& parameter : capability.parameters) {
                if (parameter.name == "mesh") {
                    has_mesh_enum = parameter.enum_values.size() == 5 &&
                                    parameter.enum_values.front() == "Cube" &&
                                    parameter.enum_values.back() == "Cone";
                }
            }
            context.expect(has_mesh_enum, "primitive capability should expose builtin mesh enum");
        }
        if (capability.op == "save") {
            has_save = true;
            context.expect(capability.requires_confirmation,
                           "save capability should require confirmation");
            context.expect(luna::authoring::hasAuthoringCommandEffect(capability.effects,
                                                                      luna::authoring::AuthoringCommandEffect::WritesFileSystem),
                           "save capability should declare filesystem write");
        }
        if (capability.op == "verify") {
            has_verify = true;
            bool has_check_enum = false;
            for (const luna::authoring::AuthoringCapabilityParameter& parameter : capability.parameters) {
                if (parameter.name == "check") {
                    has_check_enum = parameter.enum_values.size() == 4 &&
                                     parameter.enum_values.front() == "sceneSaved";
                }
            }
            context.expect(has_check_enum, "verify capability should expose check enum");
        }
        if (capability.op == "snapshot") {
            has_snapshot = true;
            context.expect(luna::authoring::hasAuthoringCommandEffect(capability.effects,
                                                                      luna::authoring::AuthoringCommandEffect::ReadsScene),
                           "snapshot capability should declare scene read");
            context.expect(!luna::authoring::hasAuthoringCommandEffect(capability.effects,
                                                                       luna::authoring::AuthoringCommandEffect::MutatesScene),
                           "snapshot capability should not declare scene mutation");
        }
    }

    context.expect(has_primitive, "default authoring capabilities should include primitive");
    context.expect(has_save, "default authoring capabilities should include save");
    context.expect(has_verify, "default authoring capabilities should include verify");
    context.expect(has_snapshot, "default authoring capabilities should include snapshot");
}

void testAuthoringCapabilitiesJson(TestContext& context)
{
    const luna::authoring::Json root = luna::authoring::defaultAuthoringCapabilitiesJson();

    std::ostringstream stream;
    stream << root.dump(2, ' ', false, luna::authoring::Json::error_handler_t::replace) << '\n';
    expectGoldenText(context,
                     stream.str(),
                     authoringFixturePath("Golden/authoring-capabilities.golden.json"),
                     "capabilities JSON should match the golden snapshot");

    context.expect(root["protocol"]["name"].get<std::string>() == std::string(luna::authoring::kAuthoringProtocolName),
                   "capabilities JSON should include protocol name");
    context.expect(root["protocol"]["version"].get<uint32_t>() == luna::authoring::kAuthoringProtocolVersion,
                   "capabilities JSON should include protocol version");
    context.expect(root["capabilities"].is_array() && root["capabilities"].size() == 21,
                   "capabilities JSON should include current capability count");

    bool found_primitive = false;
    for (const auto& capability : root["capabilities"]) {
        if (capability["op"].get<std::string>() == "primitive") {
            found_primitive = true;
            context.expect(capability["parameters"]["mesh"]["enum"].is_array() &&
                                capability["parameters"]["mesh"]["enum"].size() == 5,
                            "primitive capability JSON should include mesh enum");
            context.expect(capability["effects"].is_array() && capability["effects"].size() == 1 &&
                                capability["effects"][0].get<std::string>() == "mutatesScene",
                            "primitive capability JSON should include effects");
        }
    }
    context.expect(found_primitive, "capabilities JSON should include primitive capability");
}

void testAuthoringProtocolDiscoveryContract(TestContext& context)
{
    YAML::Node plan_schema;
    try {
        plan_schema = YAML::LoadFile(authoringSchemaPath("authoring-plan.schema.json").string());
    } catch (const YAML::Exception& error) {
        context.expect(false, std::string("authoring plan schema should parse for discovery contract: ") + error.what());
        return;
    }

    const std::vector<luna::authoring::AuthoringCapability> capabilities =
        luna::authoring::defaultAuthoringCapabilities();
    const std::set<std::string> command_names = uniqueAuthoringCommandNames();
    const std::set<std::string> schema_ops = schemaCommandOps(plan_schema);
    const std::set<std::string> discovered_ops = capabilityOps(capabilities);

    context.expect(command_names.size() == 21,
                   "unique C++ authoring command names should match current protocol op count");
    context.expect(schema_ops == command_names,
                   "plan schema command ops should match C++ authoring command names");
    context.expect(discovered_ops == command_names,
                   "default capabilities should expose every C++ authoring command op");

    std::map<std::string, size_t> op_counts;
    for (const luna::authoring::AuthoringCapability& capability : capabilities) {
        ++op_counts[capability.op];
        context.expect(!capability.title.empty(), "capability should include a title");
        context.expect(!capability.description.empty(), "capability should include a description");
        context.expect(!capability.examples.empty(), "capability should include at least one example");

        for (const luna::authoring::AuthoringCapabilityExample& example : capability.examples) {
            const std::string plan_text = std::string(R"({ "commands": [)") + example.plan_json + R"(] })";
            luna::authoring::AuthoringPlan plan;
            std::vector<std::string> errors;
            std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
            std::istringstream input(plan_text);
            context.expect(luna::authoring::loadAuthoringPlanJson(input, plan, errors, &diagnostics),
                           "capability examples should parse as authoring plan commands");
            context.expect(errors.empty(), "capability examples should not report parse errors");
            context.expect(diagnostics.empty(), "capability examples should not report diagnostics");
            if (!plan.commands.empty()) {
                context.expect(luna::authoring::authoringCommandName(plan.commands.front().kind) == capability.op,
                               "capability example op should match capability op");
            }
        }
    }

    for (const auto& [op, count] : op_counts) {
        context.expect(count == 1, "capability ops should be unique");
    }
}

void testAuthoringHostWireContract(TestContext& context)
{
    YAML::Node wire_schema;
    try {
        wire_schema = YAML::LoadFile(authoringSchemaPath("authoring-host-wire.schema.json").string());
    } catch (const YAML::Exception& error) {
        context.expect(false, std::string("authoring host wire schema should parse: ") + error.what());
        return;
    }

    const luna::authoring::Json manifest = luna::authoring::authoringHostWireManifestJson();
    const std::set<std::string> schema_methods = schemaHostWireMethodNames(wire_schema);
    const std::set<std::string> manifest_methods = manifestHostWireMethodNames(manifest);
    std::vector<std::string> schema_method_order;
    if (wire_schema["$defs"]["method"]["properties"]["name"]["enum"].IsSequence()) {
        for (const YAML::Node method_name : wire_schema["$defs"]["method"]["properties"]["name"]["enum"]) {
            if (method_name.IsScalar()) {
                schema_method_order.push_back(method_name.as<std::string>());
            }
        }
    }
    std::vector<std::string> manifest_method_order;
    if (manifest.contains("methods") && manifest["methods"].is_array()) {
        for (const auto& method : manifest["methods"]) {
            if (method.contains("name") && method["name"].is_string()) {
                manifest_method_order.push_back(method["name"].get<std::string>());
            }
        }
    }

    const std::vector<std::string> expected_methods{
        "capabilities",
        "session",
        "executePlan",
        "beginTransaction",
        "commitTransaction",
        "rollbackTransaction",
        "snapshot",
        "undo",
        "redo",
        "events",
        "clearAliases",
        "clearHistory",
        "shutdown",
    };

    context.expect(wire_schema["$schema"].as<std::string>() == "https://json-schema.org/draft/2020-12/schema",
                   "host wire schema should declare draft 2020-12");
    context.expect(wire_schema["$id"].as<std::string>() == "https://luna.local/schemas/authoring-host-wire.schema.json",
                   "host wire schema should declare a stable schema id");
    context.expect(wire_schema["$defs"]["protocol"]["properties"]["name"]["const"].as<std::string>() ==
                       "luna.authoring.host",
                   "host wire schema should declare the host protocol name");
    context.expect(wire_schema["$defs"]["protocol"]["properties"]["version"]["const"].as<uint32_t>() == 1,
                   "host wire schema should declare the host protocol version");
    context.expect(wire_schema["properties"]["transport"]["const"].as<std::string>() == "stdio-jsonrpc",
                   "host wire schema should declare stdio JSON-RPC transport");
    context.expect(wire_schema["properties"]["framing"]["const"].as<std::string>() == "newline-delimited",
                   "host wire schema should declare newline framing");
    context.expect(wire_schema["properties"]["rpcVersion"]["const"].as<std::string>() == "2.0",
                   "host wire schema should declare JSON-RPC 2.0");
    context.expect(schema_methods.size() == 13, "host wire schema should list the current method count");
    context.expect(schema_methods == manifest_methods,
                   "host wire schema method names should match the C++ manifest");
    context.expect(schema_method_order == expected_methods,
                   "host wire schema should preserve a stable method order");
    context.expect(manifest["protocol"]["name"].get<std::string>() == "luna.authoring.host",
                   "host wire manifest should declare the host protocol name");
    context.expect(manifest["protocol"]["version"].get<uint32_t>() == 1,
                   "host wire manifest should declare the host protocol version");
    context.expect(manifest["transport"].get<std::string>() == "stdio-jsonrpc",
                   "host wire manifest should declare stdio JSON-RPC transport");
    context.expect(manifest["framing"].get<std::string>() == "newline-delimited",
                   "host wire manifest should declare newline framing");
    context.expect(manifest["rpcVersion"].get<std::string>() == "2.0",
                   "host wire manifest should declare JSON-RPC version 2.0");
    context.expect(manifest["methods"].is_array() && manifest["methods"].size() == 13,
                   "host wire manifest should expose all supported methods");

    if (manifest["methods"].is_array()) {
        context.expect(manifest["methods"].size() == expected_methods.size(),
                       "host wire manifest should preserve the expected method order");
        for (size_t index = 0; index < manifest["methods"].size() && index < expected_methods.size(); ++index) {
            const auto& method = manifest["methods"][index];
            context.expect(method["name"].get<std::string>() == expected_methods[index],
                           "host wire manifest should preserve a stable method order");
            context.expect(method["requestShape"].is_string() && !method["requestShape"].get<std::string>().empty(),
                           "host wire manifest should describe each request shape");
            context.expect(method["resultShape"].is_string() && !method["resultShape"].get<std::string>().empty(),
                           "host wire manifest should describe each result shape");
        }
    }
    context.expect(manifest_method_order == expected_methods,
                   "host wire manifest should preserve a stable method order");
}

void testAuthoringPlanJsonFixtures(TestContext& context)
{
    struct ValidFixture {
        const char* filename;
        size_t command_count;
    };

    const ValidFixture valid_fixtures[]{
        {"create_cube_scene.plan.json", 10},
        {"lights_camera.plan.json", 12},
    };

    for (const ValidFixture& fixture : valid_fixtures) {
        const std::filesystem::path path = authoringFixturePath(fixture.filename);
        luna::authoring::AuthoringPlan plan;
        std::vector<std::string> errors;
        std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
        context.expect(luna::authoring::loadAuthoringPlanJson(path, plan, errors, &diagnostics),
                       "valid authoring fixture should parse");
        context.expect(errors.empty(), "valid authoring fixture should not report errors");
        context.expect(diagnostics.empty(), "valid authoring fixture should not report diagnostics");
        context.expect(plan.protocol.name == std::string(luna::authoring::kAuthoringProtocolName),
                       "valid authoring fixture should use the current protocol name");
        context.expect(plan.protocol.version == luna::authoring::kAuthoringProtocolVersion,
                       "valid authoring fixture should use the current protocol version");
        context.expect(plan.commands.size() == fixture.command_count,
                       "valid authoring fixture should preserve command count");

        std::ostringstream stream;
        luna::authoring::writeAuthoringPlanJson(stream, plan);

        luna::authoring::AuthoringPlan reparsed_plan;
        std::vector<std::string> reparse_errors;
        std::vector<luna::authoring::AuthoringDiagnostic> reparse_diagnostics;
        std::istringstream input(stream.str());
        context.expect(luna::authoring::loadAuthoringPlanJson(input,
                                                              reparsed_plan,
                                                              reparse_errors,
                                                              &reparse_diagnostics),
                       "valid authoring fixture should round-trip through the shared writer");
        context.expect(reparse_errors.empty(), "round-tripped authoring fixture should not report errors");
        context.expect(reparse_diagnostics.empty(),
                       "round-tripped authoring fixture should not report diagnostics");
        context.expect(reparsed_plan.commands.size() == plan.commands.size(),
                       "round-tripped authoring fixture should preserve command count");
    }
}

void testAuthoringPlanJsonInvalidFixtures(TestContext& context)
{
    struct InvalidFixture {
        const char* filename;
        luna::authoring::AuthoringDiagnosticCode code;
        const char* field;
    };

    const InvalidFixture invalid_fixtures[]{
        {
            "invalid_unknown_command.plan.json",
            luna::authoring::AuthoringDiagnosticCode::UnsupportedCommand,
            "commands[0].op",
        },
        {
            "invalid_missing_transform.plan.json",
            luna::authoring::AuthoringDiagnosticCode::InvalidPlan,
            "commands[0].translation",
        },
    };

    for (const InvalidFixture& fixture : invalid_fixtures) {
        const std::filesystem::path path = authoringFixturePath(fixture.filename);
        luna::authoring::AuthoringPlan plan;
        std::vector<std::string> errors;
        std::vector<luna::authoring::AuthoringDiagnostic> diagnostics;
        context.expect(!luna::authoring::loadAuthoringPlanJson(path, plan, errors, &diagnostics),
                       "invalid authoring fixture should fail");
        context.expect(!errors.empty(), "invalid authoring fixture should report errors");
        context.expect(diagnostics.size() == 1, "invalid authoring fixture should report one diagnostic");
        if (!diagnostics.empty()) {
            context.expect(diagnostics.front().code == fixture.code,
                           "invalid authoring fixture should report expected diagnostic code");
            context.expect(diagnostics.front().phase == luna::authoring::AuthoringDiagnosticPhase::Validate,
                           "invalid authoring fixture should report a validate diagnostic");
            context.expect(diagnostics.front().field == fixture.field,
                           "invalid authoring fixture should report expected field");
            context.expect(diagnostics.front().path == path,
                           "invalid authoring fixture diagnostic should include source path");
        }
    }
}

void testAuthoringReportJson(TestContext& context)
{
    luna::authoring::AuthoringReport report;
    report.scene = {
        .name = "Protocol Scene",
        .path = "Generated/ProtocolScene.lunascene",
        .entity_count = 3,
        .dirty = false,
    };
    report.entities.push_back({
        .alias = "Box",
        .entity_id = luna::UUID(42),
        .name = "Cube",
    });
    report.verifications.push_back({
        .kind = luna::authoring::AuthoringVerificationKind::EntityExists,
        .ok = true,
        .ref = "Box",
        .entity_id = luna::UUID(42),
        .message = "Entity exists.",
    });
    luna::authoring::appendAuthoringDiagnostic(report,
                                               {
                                                   .severity = luna::authoring::AuthoringDiagnosticSeverity::Error,
                                                   .phase = luna::authoring::AuthoringDiagnosticPhase::Execute,
                                                   .code = luna::authoring::AuthoringDiagnosticCode::MissingComponent,
                                                   .has_command_index = true,
                                                   .command_index = 2,
                                                   .recoverable = true,
                                                   .command = "transform",
                                                   .entity_ref = "Box",
                                                   .component = "Transform",
                                                   .expected = "Transform component",
                                                   .actual = "missing Transform component",
                                                   .suggested_command = "transform Box ...",
                                                   .message = "Entity does not have a Transform component.",
                                               });

    const luna::authoring::Json root = luna::authoring::authoringReportJson(report, false);

    std::ostringstream stream;
    stream << root.dump(2, ' ', false, luna::authoring::Json::error_handler_t::replace) << '\n';
    expectGoldenText(context,
                     stream.str(),
                     authoringFixturePath("Golden/authoring-report.error.golden.json"),
                     "report JSON should match the golden snapshot");

    context.expect(root["protocol"]["name"].get<std::string>() == std::string(luna::authoring::kAuthoringProtocolName),
                   "report JSON should include protocol name");
    context.expect(root["protocol"]["version"].get<uint32_t>() == luna::authoring::kAuthoringProtocolVersion,
                   "report JSON should include protocol version");
    context.expect(!root["ok"].get<bool>(), "report JSON should include ok flag");
    context.expect(root["scene"]["entityCount"].get<size_t>() == 3,
                   "report JSON should include scene entity count");
    context.expect(root["entities"].is_array() && root["entities"].size() == 1,
                   "report JSON should include entity bindings");
    context.expect(root["verifications"].is_array() && root["verifications"].size() == 1,
                   "report JSON should include verifications");
    context.expect(root["diagnostics"].is_array() && root["diagnostics"].size() == 1,
                   "report JSON should include diagnostics");
    if (root["diagnostics"].is_array() && root["diagnostics"].size() == 1) {
        const auto diagnostic = root["diagnostics"][0];
        context.expect(diagnostic["code"].get<std::string>() == "MissingComponent",
                       "diagnostic JSON should include stable code");
        context.expect(diagnostic["phase"].get<std::string>() == "execute",
                       "diagnostic JSON should include phase");
        context.expect(diagnostic["severity"].get<std::string>() == "error",
                       "diagnostic JSON should include severity");
        context.expect(diagnostic["commandIndex"].get<size_t>() == 2,
                       "diagnostic JSON should include command index");
        context.expect(diagnostic["entityRef"].get<std::string>() == "Box",
                       "diagnostic JSON should include entity ref");
        context.expect(diagnostic["component"].get<std::string>() == "Transform",
                       "diagnostic JSON should include component");
        context.expect(diagnostic["recoverable"].get<bool>(), "diagnostic JSON should include recoverable flag");
        context.expect(diagnostic["expected"].get<std::string>() == "Transform component",
                       "diagnostic JSON should include expected field");
        context.expect(diagnostic["actual"].get<std::string>() == "missing Transform component",
                       "diagnostic JSON should include actual field");
        context.expect(diagnostic["suggestedCommand"].get<std::string>() == "transform Box ...",
                       "diagnostic JSON should include suggested command");
    }
    context.expect(root["errors"].is_array() && root["errors"].size() == 1,
                   "diagnostic errors should still be mirrored into errors array");
}

} // namespace

int main()
{
    TestContext context;
    testCommandTokenParsing(context);
    testCommandTokenParseErrors(context);
    testCommandEffectClassification(context);
    testAuthoringPlanJsonRoundTrip(context);
    testAuthoringPlanJsonAcceptsMissingProtocol(context);
    testAuthoringPlanJsonReportsProtocolMismatch(context);
    testAuthoringPlanJsonRejectsUnknownFields(context);
    testAuthoringPlanJsonAcceptsLegacySavedCheck(context);
    testAuthoringPlanJsonParseDiagnostics(context);
    testAuthoringJsonSchemasParse(context);
    testAuthoringCapabilities(context);
    testAuthoringCapabilitiesJson(context);
    testAuthoringProtocolDiscoveryContract(context);
    testAuthoringHostWireContract(context);
    testAuthoringPlanJsonFixtures(context);
    testAuthoringPlanJsonInvalidFixtures(context);
    testAuthoringReportJson(context);
    return context.result();
}
