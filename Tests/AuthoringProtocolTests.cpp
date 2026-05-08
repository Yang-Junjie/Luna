#include "Authoring/AuthoringJson.h"
#include "Authoring/AuthoringPlanJson.h"
#include "Authoring/AuthoringProtocol.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
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
    };

    luna::authoring::AuthoringPlan plan;
    std::vector<std::string> errors;
    context.expect(luna::authoring::parseAuthoringCommandTokens(tokens, plan, errors),
                   "valid authoring tokens should parse");
    context.expect(errors.empty(), "valid authoring tokens should not report parse errors");
    context.expect(plan.commands.size() == 5, "valid authoring tokens should produce five commands");
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

    const std::vector<AuthoringCommandKind> command_kinds{
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
        AuthoringCommandKind::Summary,
    };

    size_t file_read_count = 0;
    size_t file_write_count = 0;
    for (const AuthoringCommandKind kind : command_kinds) {
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
    for (const char* filename : {"authoring-plan.schema.json", "authoring-report.schema.json"}) {
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

void testAuthoringPlanJsonFixtures(TestContext& context)
{
    struct ValidFixture {
        const char* filename;
        size_t command_count;
    };

    const ValidFixture valid_fixtures[]{
        {"create_cube_scene.plan.json", 9},
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

    std::ostringstream stream;
    luna::authoring::writeAuthoringReportJson(stream, report, false);

    YAML::Node root;
    try {
        root = YAML::Load(stream.str());
    } catch (const YAML::Exception& error) {
        context.expect(false, std::string("report JSON should parse: ") + error.what());
        return;
    }

    context.expect(root["protocol"]["name"].as<std::string>() == std::string(luna::authoring::kAuthoringProtocolName),
                   "report JSON should include protocol name");
    context.expect(root["protocol"]["version"].as<uint32_t>() == luna::authoring::kAuthoringProtocolVersion,
                   "report JSON should include protocol version");
    context.expect(!root["ok"].as<bool>(), "report JSON should include ok flag");
    context.expect(root["scene"]["entityCount"].as<size_t>() == 3,
                   "report JSON should include scene entity count");
    context.expect(root["entities"].IsSequence() && root["entities"].size() == 1,
                   "report JSON should include entity bindings");
    context.expect(root["verifications"].IsSequence() && root["verifications"].size() == 1,
                   "report JSON should include verifications");
    context.expect(root["diagnostics"].IsSequence() && root["diagnostics"].size() == 1,
                   "report JSON should include diagnostics");
    if (root["diagnostics"].IsSequence() && root["diagnostics"].size() == 1) {
        const YAML::Node diagnostic = root["diagnostics"][0];
        context.expect(diagnostic["code"].as<std::string>() == "MissingComponent",
                       "diagnostic JSON should include stable code");
        context.expect(diagnostic["phase"].as<std::string>() == "execute",
                       "diagnostic JSON should include phase");
        context.expect(diagnostic["severity"].as<std::string>() == "error",
                       "diagnostic JSON should include severity");
        context.expect(diagnostic["commandIndex"].as<size_t>() == 2,
                       "diagnostic JSON should include command index");
        context.expect(diagnostic["entityRef"].as<std::string>() == "Box",
                       "diagnostic JSON should include entity ref");
        context.expect(diagnostic["component"].as<std::string>() == "Transform",
                       "diagnostic JSON should include component");
        context.expect(diagnostic["recoverable"].as<bool>(), "diagnostic JSON should include recoverable flag");
        context.expect(diagnostic["expected"].as<std::string>() == "Transform component",
                       "diagnostic JSON should include expected field");
        context.expect(diagnostic["actual"].as<std::string>() == "missing Transform component",
                       "diagnostic JSON should include actual field");
        context.expect(diagnostic["suggestedCommand"].as<std::string>() == "transform Box ...",
                       "diagnostic JSON should include suggested command");
    }
    context.expect(root["errors"].IsSequence() && root["errors"].size() == 1,
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
    testAuthoringPlanJsonAcceptsLegacySavedCheck(context);
    testAuthoringPlanJsonParseDiagnostics(context);
    testAuthoringJsonSchemasParse(context);
    testAuthoringPlanJsonFixtures(context);
    testAuthoringPlanJsonInvalidFixtures(context);
    testAuthoringReportJson(context);
    return context.result();
}
