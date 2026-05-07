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
    context.expect(!luna::authoring::parseAuthoringCommandTokens(tokens, plan, errors),
                   "invalid authoring tokens should fail");
    context.expect(!errors.empty(), "invalid authoring tokens should report parse errors");
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

    std::ostringstream stream;
    luna::authoring::writeAuthoringReportJson(stream, report, true);

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
    context.expect(root["ok"].as<bool>(), "report JSON should include ok flag");
    context.expect(root["scene"]["entityCount"].as<size_t>() == 3,
                   "report JSON should include scene entity count");
    context.expect(root["entities"].IsSequence() && root["entities"].size() == 1,
                   "report JSON should include entity bindings");
    context.expect(root["verifications"].IsSequence() && root["verifications"].size() == 1,
                   "report JSON should include verifications");
}

} // namespace

int main()
{
    TestContext context;
    testCommandTokenParsing(context);
    testCommandTokenParseErrors(context);
    testAuthoringReportJson(context);
    return context.result();
}
