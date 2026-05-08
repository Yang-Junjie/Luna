#include "Authoring/AuthoringJson.h"
#include "Authoring/AuthoringProtocol.h"

#include <yaml-cpp/yaml.h>

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
    testAuthoringReportJson(context);
    return context.result();
}
