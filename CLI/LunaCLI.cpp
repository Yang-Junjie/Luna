#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Authoring/AuthoringExecutor.h"
#include "Authoring/AuthoringJson.h"
#include "Authoring/AuthoringProtocol.h"
#include "Authoring/AuthoringSession.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Scene/Scene.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct CliState {
    luna::Scene scene;
    luna::authoring::AuthoringSession session{scene};
    luna::authoring::AuthoringReport report;
    bool json_output{false};
};

struct ParsedCliOptions {
    bool help{false};
    bool json_output{false};
    std::filesystem::path project_file_path;
    std::vector<std::string> commands;
    std::vector<std::string> errors;
};

void printUsage()
{
    std::cout
        << "LunaCLI\n"
        << "\n"
        << "Usage:\n"
        << "  LunaCLI [--json] [--project <path>] <commands...>\n"
        << "  LunaCLI [--json] [--project <path>] plan <plan.json>\n"
        << "\n"
        << "Commands run left-to-right:\n"
        << "  new\n"
        << "  open <scene-path>\n"
        << "  save <scene-path>\n"
        << "  entity <alias> <name>\n"
        << "  camera <alias>\n"
        << "  directional-light <alias>\n"
        << "  point-light <alias>\n"
        << "  spot-light <alias>\n"
        << "  primitive <alias> <Cube|Sphere|Plane|Cylinder|Cone>\n"
        << "  parent <child-ref> <parent-ref>\n"
        << "  unparent <child-ref>\n"
        << "  name <entity-ref> <name>\n"
        << "  transform <entity-ref> tx ty tz rxDeg ryDeg rzDeg sx sy sz\n"
        << "  light-intensity <entity-ref> <value>\n"
        << "  light-color <entity-ref> r g b\n"
        << "  camera-perspective <entity-ref> fovDeg near far\n"
        << "  camera-orthographic <entity-ref> size near far\n"
        << "  inspect scene\n"
        << "  inspect entity <entity-ref>\n"
        << "  inspect hierarchy\n"
        << "  verify saved\n"
        << "  verify entity <entity-ref>\n"
        << "  verify component <entity-ref> <component>\n"
        << "  verify entity-count-at-least <count>\n"
        << "  summary\n"
        << "\n"
        << "Entity refs are aliases created earlier in the same command, or numeric UUIDs.\n";
}

void reportError(CliState& state, std::string message)
{
    state.report.errors.push_back(message);
    if (!state.json_output) {
        std::cerr << message << '\n';
    }
}

ParsedCliOptions parseCliOptions(const std::vector<std::string>& args)
{
    ParsedCliOptions options;

    for (size_t index = 0; index < args.size();) {
        const std::string& arg = args[index];
        if (arg == "--help" || arg == "-h" || arg == "help") {
            options.help = true;
            return options;
        }

        if (arg == "--json") {
            options.json_output = true;
            ++index;
            continue;
        }

        if (arg == "--project") {
            if (index + 1 >= args.size()) {
                options.errors.emplace_back("Missing value for --project.");
                return options;
            }
            options.project_file_path = args[index + 1];
            index += 2;
            continue;
        }

        constexpr std::string_view project_prefix = "--project=";
        if (arg.starts_with(project_prefix)) {
            const std::string value = arg.substr(project_prefix.size());
            if (value.empty()) {
                options.errors.emplace_back("Missing value for --project.");
                return options;
            }
            options.project_file_path = value;
            ++index;
            continue;
        }

        options.commands.assign(args.begin() + static_cast<std::ptrdiff_t>(index), args.end());
        return options;
    }

    return options;
}

std::string toCliNumberToken(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return stream.str();
}

std::string planFieldName(size_t command_index, std::string_view field_name)
{
    std::ostringstream stream;
    stream << "commands[" << command_index << "]." << field_name;
    return stream.str();
}

bool readPlanStringNode(CliState& state, const YAML::Node& node, const std::string& field_name, std::string& value)
{
    if (!node || !node.IsScalar()) {
        reportError(state, "Plan field '" + field_name + "' must be a non-empty string.");
        return false;
    }

    try {
        value = node.as<std::string>();
    } catch (const YAML::Exception&) {
        reportError(state, "Plan field '" + field_name + "' must be a non-empty string.");
        return false;
    }

    if (value.empty()) {
        reportError(state, "Plan field '" + field_name + "' must be a non-empty string.");
        return false;
    }

    return true;
}

bool readPlanStringField(CliState& state,
                         const YAML::Node& node,
                         const char* key,
                         const std::string& field_name,
                         std::string& value)
{
    return readPlanStringNode(state, node[key], field_name, value);
}

bool readPlanNumberNode(CliState& state, const YAML::Node& node, const std::string& field_name, double& value)
{
    if (!node || !node.IsScalar()) {
        reportError(state, "Plan field '" + field_name + "' must be a finite number.");
        return false;
    }

    try {
        value = node.as<double>();
    } catch (const YAML::Exception&) {
        reportError(state, "Plan field '" + field_name + "' must be a finite number.");
        return false;
    }

    if (!std::isfinite(value)) {
        reportError(state, "Plan field '" + field_name + "' must be a finite number.");
        return false;
    }

    return true;
}

bool readPlanNumberField(CliState& state,
                         const YAML::Node& node,
                         const char* key,
                         const std::string& field_name,
                         double& value)
{
    return readPlanNumberNode(state, node[key], field_name, value);
}

bool appendPlanVec3(CliState& state,
                    std::vector<std::string>& args,
                    const YAML::Node& node,
                    const std::string& field_name)
{
    if (!node || !node.IsSequence() || node.size() != 3) {
        reportError(state, "Plan field '" + field_name + "' must be a 3-number array.");
        return false;
    }

    for (size_t index = 0; index < 3; ++index) {
        double value = 0.0;
        if (!readPlanNumberNode(state, node[index], field_name + "[" + std::to_string(index) + "]", value)) {
            return false;
        }
        args.push_back(toCliNumberToken(value));
    }

    return true;
}

void appendPlanVec3Fallback(std::vector<std::string>& args, double x, double y, double z)
{
    args.push_back(toCliNumberToken(x));
    args.push_back(toCliNumberToken(y));
    args.push_back(toCliNumberToken(z));
}

bool appendPlanCommand(CliState& state,
                       const YAML::Node& command_node,
                       size_t command_index,
                       std::vector<std::string>& args)
{
    if (!command_node || !command_node.IsMap()) {
        reportError(state, "Plan command " + std::to_string(command_index) + " must be an object.");
        return false;
    }

    std::string op;
    if (!readPlanStringField(state, command_node, "op", planFieldName(command_index, "op"), op)) {
        return false;
    }

    if (op == "new" || op == "summary") {
        args.push_back(op);
        return true;
    }

    if (op == "open" || op == "save") {
        std::string path;
        if (!readPlanStringField(state, command_node, "path", planFieldName(command_index, "path"), path)) {
            return false;
        }
        args.push_back(op);
        args.push_back(path);
        return true;
    }

    if (op == "entity") {
        std::string alias;
        std::string name;
        if (!readPlanStringField(state, command_node, "alias", planFieldName(command_index, "alias"), alias) ||
            !readPlanStringField(state, command_node, "name", planFieldName(command_index, "name"), name)) {
            return false;
        }
        args.push_back(op);
        args.push_back(alias);
        args.push_back(name);
        return true;
    }

    if (op == "camera" || op == "directional-light" || op == "point-light" || op == "spot-light") {
        std::string alias;
        if (!readPlanStringField(state, command_node, "alias", planFieldName(command_index, "alias"), alias)) {
            return false;
        }
        args.push_back(op);
        args.push_back(alias);
        return true;
    }

    if (op == "primitive") {
        std::string alias;
        std::string mesh;
        if (!readPlanStringField(state, command_node, "alias", planFieldName(command_index, "alias"), alias) ||
            !readPlanStringField(state, command_node, "mesh", planFieldName(command_index, "mesh"), mesh)) {
            return false;
        }
        args.push_back("primitive");
        args.push_back(alias);
        args.push_back(mesh);
        return true;
    }

    if (op == "parent") {
        std::string child;
        std::string parent;
        if (!readPlanStringField(state, command_node, "child", planFieldName(command_index, "child"), child) ||
            !readPlanStringField(state, command_node, "parent", planFieldName(command_index, "parent"), parent)) {
            return false;
        }
        args.push_back("parent");
        args.push_back(child);
        args.push_back(parent);
        return true;
    }

    if (op == "unparent") {
        std::string child;
        if (!readPlanStringField(state, command_node, "child", planFieldName(command_index, "child"), child)) {
            return false;
        }
        args.push_back("unparent");
        args.push_back(child);
        return true;
    }

    if (op == "name") {
        std::string entity;
        std::string name;
        if (!readPlanStringField(state, command_node, "entity", planFieldName(command_index, "entity"), entity) ||
            !readPlanStringField(state, command_node, "name", planFieldName(command_index, "name"), name)) {
            return false;
        }
        args.push_back("name");
        args.push_back(entity);
        args.push_back(name);
        return true;
    }

    if (op == "transform") {
        std::string entity;
        if (!readPlanStringField(state, command_node, "entity", planFieldName(command_index, "entity"), entity)) {
            return false;
        }

        args.push_back("transform");
        args.push_back(entity);
        if (!appendPlanVec3(state, args, command_node["translation"], planFieldName(command_index, "translation"))) {
            return false;
        }

        if (const YAML::Node rotation = command_node["rotationDeg"]) {
            if (!appendPlanVec3(state, args, rotation, planFieldName(command_index, "rotationDeg"))) {
                return false;
            }
        } else {
            appendPlanVec3Fallback(args, 0.0, 0.0, 0.0);
        }

        if (const YAML::Node scale = command_node["scale"]) {
            if (!appendPlanVec3(state, args, scale, planFieldName(command_index, "scale"))) {
                return false;
            }
        } else {
            appendPlanVec3Fallback(args, 1.0, 1.0, 1.0);
        }
        return true;
    }

    if (op == "light-intensity") {
        std::string entity;
        double value = 0.0;
        if (!readPlanStringField(state, command_node, "entity", planFieldName(command_index, "entity"), entity) ||
            !readPlanNumberField(state, command_node, "value", planFieldName(command_index, "value"), value)) {
            return false;
        }
        args.push_back("light-intensity");
        args.push_back(entity);
        args.push_back(toCliNumberToken(value));
        return true;
    }

    if (op == "light-color") {
        std::string entity;
        if (!readPlanStringField(state, command_node, "entity", planFieldName(command_index, "entity"), entity)) {
            return false;
        }
        args.push_back("light-color");
        args.push_back(entity);
        return appendPlanVec3(state, args, command_node["color"], planFieldName(command_index, "color"));
    }

    if (op == "camera-perspective") {
        std::string entity;
        double fov = 0.0;
        double near_plane = 0.0;
        double far_plane = 0.0;
        if (!readPlanStringField(state, command_node, "entity", planFieldName(command_index, "entity"), entity) ||
            !readPlanNumberField(state, command_node, "fovDeg", planFieldName(command_index, "fovDeg"), fov) ||
            !readPlanNumberField(state, command_node, "near", planFieldName(command_index, "near"), near_plane) ||
            !readPlanNumberField(state, command_node, "far", planFieldName(command_index, "far"), far_plane)) {
            return false;
        }
        args.push_back("camera-perspective");
        args.push_back(entity);
        args.push_back(toCliNumberToken(fov));
        args.push_back(toCliNumberToken(near_plane));
        args.push_back(toCliNumberToken(far_plane));
        return true;
    }

    if (op == "camera-orthographic") {
        std::string entity;
        double size = 0.0;
        double near_plane = 0.0;
        double far_plane = 0.0;
        if (!readPlanStringField(state, command_node, "entity", planFieldName(command_index, "entity"), entity) ||
            !readPlanNumberField(state, command_node, "size", planFieldName(command_index, "size"), size) ||
            !readPlanNumberField(state, command_node, "near", planFieldName(command_index, "near"), near_plane) ||
            !readPlanNumberField(state, command_node, "far", planFieldName(command_index, "far"), far_plane)) {
            return false;
        }
        args.push_back("camera-orthographic");
        args.push_back(entity);
        args.push_back(toCliNumberToken(size));
        args.push_back(toCliNumberToken(near_plane));
        args.push_back(toCliNumberToken(far_plane));
        return true;
    }

    if (op == "inspect") {
        std::string target;
        if (!readPlanStringField(state, command_node, "target", planFieldName(command_index, "target"), target)) {
            return false;
        }
        args.push_back("inspect");
        args.push_back(target);
        if (target == "entity") {
            std::string entity;
            if (!readPlanStringField(state,
                                     command_node,
                                     "entity",
                                     planFieldName(command_index, "entity"),
                                     entity)) {
                return false;
            }
            args.push_back(entity);
        }
        return true;
    }

    if (op == "verify") {
        std::string check;
        if (!readPlanStringField(state, command_node, "check", planFieldName(command_index, "check"), check)) {
            return false;
        }
        args.push_back("verify");
        if (check == "sceneSaved") {
            args.push_back("saved");
            return true;
        }
        if (check == "entityExists") {
            std::string entity;
            if (!readPlanStringField(state,
                                     command_node,
                                     "entity",
                                     planFieldName(command_index, "entity"),
                                     entity)) {
                return false;
            }
            args.push_back("entity");
            args.push_back(entity);
            return true;
        }
        if (check == "hasComponent") {
            std::string entity;
            std::string component;
            if (!readPlanStringField(state,
                                     command_node,
                                     "entity",
                                     planFieldName(command_index, "entity"),
                                     entity) ||
                !readPlanStringField(state,
                                     command_node,
                                     "component",
                                     planFieldName(command_index, "component"),
                                     component)) {
                return false;
            }
            args.push_back("component");
            args.push_back(entity);
            args.push_back(component);
            return true;
        }
        if (check == "entityCountAtLeast") {
            double count = 0.0;
            if (!readPlanNumberField(state, command_node, "count", planFieldName(command_index, "count"), count) ||
                count < 0.0 || std::floor(count) != count) {
                reportError(state, "Plan field '" + planFieldName(command_index, "count") + "' must be a non-negative integer.");
                return false;
            }
            args.push_back("entity-count-at-least");
            args.push_back(toCliNumberToken(count));
            return true;
        }

        reportError(state, "Unsupported verify check '" + check + "'.");
        return false;
    }

    reportError(state, "Unsupported plan op '" + op + "'.");
    return false;
}

bool loadPlan(CliState& state, const std::filesystem::path& plan_path, luna::authoring::AuthoringPlan& plan)
{
    YAML::Node root;
    try {
        root = YAML::LoadFile(plan_path.string());
    } catch (const YAML::Exception& error) {
        reportError(state, "Failed to read plan '" + plan_path.string() + "': " + error.what());
        return false;
    }

    if (!root || !root.IsMap()) {
        reportError(state, "Plan root must be an object.");
        return false;
    }

    if (const YAML::Node protocol = root["protocol"]) {
        if (!protocol.IsMap()) {
            reportError(state, "Plan field 'protocol' must be an object.");
            return false;
        }

        std::string protocol_name;
        if (!readPlanStringNode(state, protocol["name"], "protocol.name", protocol_name)) {
            return false;
        }
        if (protocol_name != luna::authoring::kAuthoringProtocolName) {
            reportError(state, "Unsupported authoring protocol '" + protocol_name + "'.");
            return false;
        }

        double protocol_version = 0.0;
        if (!readPlanNumberNode(state, protocol["version"], "protocol.version", protocol_version) ||
            protocol_version < 0.0 || std::floor(protocol_version) != protocol_version) {
            reportError(state, "Plan field 'protocol.version' must be a non-negative integer.");
            return false;
        }
        if (static_cast<uint32_t>(protocol_version) != luna::authoring::kAuthoringProtocolVersion) {
            reportError(state, "Unsupported authoring protocol version '" + toCliNumberToken(protocol_version) + "'.");
            return false;
        }

        plan.protocol.name = protocol_name;
        plan.protocol.version = static_cast<uint32_t>(protocol_version);
    }

    if (const YAML::Node project = root["project"]) {
        std::string project_path;
        if (!readPlanStringNode(state, project, "project", project_path)) {
            return false;
        }
        plan.project_file_path = project_path;
    }

    const YAML::Node commands = root["commands"];
    if (!commands || !commands.IsSequence()) {
        reportError(state, "Plan field 'commands' must be an array.");
        return false;
    }

    std::vector<std::string> command_tokens;
    for (size_t index = 0; index < commands.size(); ++index) {
        if (!appendPlanCommand(state, commands[index], index, command_tokens)) {
            return false;
        }
    }

    std::vector<std::string> parse_errors;
    if (!luna::authoring::parseAuthoringCommandTokens(command_tokens, plan, parse_errors)) {
        for (const std::string& error : parse_errors) {
            reportError(state, error);
        }
        return false;
    }

    return true;
}

std::filesystem::path resolveProjectFilePath(const std::filesystem::path& input_path)
{
    if (input_path.empty()) {
        return {};
    }

    std::error_code ec;
    if (std::filesystem::is_directory(input_path, ec) && !ec) {
        for (std::filesystem::directory_iterator it(input_path, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code entry_ec;
            if (it->is_regular_file(entry_ec) && !entry_ec && it->path().extension() == ".lunaproj") {
                return it->path().lexically_normal();
            }
        }
        return {};
    }

    return input_path.lexically_normal();
}

void printSummary(const CliState& state)
{
    std::cout << "Scene: " << state.scene.getName() << "\n";
    std::cout << "Scene File: "
              << (state.session.sceneFilePath().empty() ? "<unsaved>" : state.session.sceneFilePath().string())
              << "\n";
    std::cout << "Entities: " << state.scene.entityManager().entityCount() << "\n";
    std::cout << "Dirty: " << (state.session.isSceneDirty() ? "true" : "false") << "\n";
}

void printEntityBindings(const luna::authoring::AuthoringReport& report)
{
    for (const luna::authoring::AuthoringEntityBinding& binding : report.entities) {
        std::cout << binding.alias << "=" << binding.entity_id.toString() << "\n";
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>((std::max)(argc - 1, 0)));
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index] != nullptr ? argv[index] : "");
    }

    const ParsedCliOptions options = parseCliOptions(args);
    if (args.empty() || options.help) {
        printUsage();
        return args.empty() ? 1 : 0;
    }

    CliState state;
    state.json_output = options.json_output;

    bool ok = true;
    for (const std::string& error : options.errors) {
        reportError(state, error);
        ok = false;
    }

    std::vector<std::string> command_args = options.commands;
    std::filesystem::path project_file_path = options.project_file_path;
    luna::authoring::AuthoringPlan plan;

    if (ok && command_args.empty()) {
        reportError(state, "No commands specified.");
        ok = false;
    }

    if (ok && !command_args.empty() && command_args[0] == "plan") {
        if (command_args.size() != 2) {
            reportError(state, "Command 'plan' expects exactly one plan path.");
            ok = false;
        } else {
            ok = loadPlan(state, command_args[1], plan);
            if (ok) {
                if (project_file_path.empty()) {
                    project_file_path = plan.project_file_path;
                }
            }
        }
    } else if (ok) {
        std::vector<std::string> parse_errors;
        ok = luna::authoring::parseAuthoringCommandTokens(command_args, plan, parse_errors);
        for (const std::string& error : parse_errors) {
            reportError(state, error);
        }
    }

    if (!ok) {
        if (state.json_output) {
            state.report.scene = luna::authoring::captureAuthoringSceneSnapshot(state.session);
            luna::authoring::writeAuthoringReportJson(std::cout, state.report, false);
        }
        return 1;
    }

    luna::Logger::init("", state.json_output ? luna::Logger::Level::Off : luna::Logger::Level::Warn);
    luna::AssetManager::get().init();

    if (!project_file_path.empty()) {
        const std::filesystem::path resolved_project_path = resolveProjectFilePath(project_file_path);
        if (resolved_project_path.empty() || !luna::ProjectManager::instance().loadProject(resolved_project_path)) {
            reportError(state, "Failed to load project '" + project_file_path.string() + "'.");
            ok = false;
        }
    }

    const size_t error_count_before_execute = state.report.errors.size();
    if (ok) {
        luna::authoring::AuthoringExecutor executor(state.session);
        ok = executor.execute(plan, state.report);
    }

    if (state.json_output) {
        state.report.scene = luna::authoring::captureAuthoringSceneSnapshot(state.session);
        luna::authoring::writeAuthoringReportJson(std::cout, state.report, ok);
    } else if (ok) {
        printEntityBindings(state.report);
        printSummary(state);
    } else {
        for (size_t index = error_count_before_execute; index < state.report.errors.size(); ++index) {
            std::cerr << state.report.errors[index] << '\n';
        }
    }

    luna::AssetManager::get().clear();
    luna::AssetDatabase::clear();
    luna::Logger::shutdown();
    return ok ? 0 : 1;
}
