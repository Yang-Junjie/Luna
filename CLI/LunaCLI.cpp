#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Authoring/AuthoringExecutor.h"
#include "Authoring/AuthoringJson.h"
#include "Authoring/AuthoringPlanJson.h"
#include "Authoring/AuthoringProtocol.h"
#include "Authoring/AuthoringSession.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
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
    bool dry_run{false};
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
        << "  LunaCLI [--json] [--dry-run] [--project <path>] <commands...>\n"
        << "  LunaCLI [--json] [--dry-run] [--project <path>] plan <plan.json>\n"
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
    luna::authoring::appendAuthoringDiagnostic(state.report,
                                               {
                                                   .severity = luna::authoring::AuthoringDiagnosticSeverity::Error,
                                                   .phase = luna::authoring::AuthoringDiagnosticPhase::Validate,
                                                   .code = luna::authoring::AuthoringDiagnosticCode::InvalidPlan,
                                                   .recoverable = true,
                                                   .message = message,
                                               });
    if (!state.json_output) {
        std::cerr << message << '\n';
    }
}

void reportDiagnostic(CliState& state, luna::authoring::AuthoringDiagnostic diagnostic)
{
    const std::string message = diagnostic.message;
    luna::authoring::appendAuthoringDiagnostic(state.report, std::move(diagnostic));
    if (!state.json_output) {
        std::cerr << message << '\n';
    }
}

void reportPlanDiagnostic(CliState& state,
                          luna::authoring::AuthoringDiagnosticCode code,
                          std::string message,
                          std::string field = {})
{
    reportDiagnostic(state,
                     {
                         .severity = luna::authoring::AuthoringDiagnosticSeverity::Error,
                         .phase = luna::authoring::AuthoringDiagnosticPhase::Validate,
                         .code = code,
                         .recoverable = true,
                         .field = std::move(field),
                         .message = std::move(message),
                     });
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

        if (arg == "--dry-run") {
            options.dry_run = true;
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

bool loadPlan(CliState& state, const std::filesystem::path& plan_path, luna::authoring::AuthoringPlan& plan)
{
    std::vector<std::string> parse_errors;
    std::vector<luna::authoring::AuthoringDiagnostic> parse_diagnostics;
    if (!luna::authoring::loadAuthoringPlanJson(plan_path, plan, parse_errors, &parse_diagnostics)) {
        if (parse_diagnostics.empty()) {
            for (const std::string& error : parse_errors) {
                reportError(state, error);
            }
        } else {
            for (luna::authoring::AuthoringDiagnostic& diagnostic : parse_diagnostics) {
                reportDiagnostic(state, std::move(diagnostic));
            }
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

void printWarnings(const luna::authoring::AuthoringReport& report)
{
    for (const luna::authoring::AuthoringDiagnostic& diagnostic : report.diagnostics) {
        if (diagnostic.severity == luna::authoring::AuthoringDiagnosticSeverity::Warning) {
            std::cerr << "Warning: " << diagnostic.message << '\n';
        }
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
        reportPlanDiagnostic(state, luna::authoring::AuthoringDiagnosticCode::MissingArgument, error);
        ok = false;
    }

    std::vector<std::string> command_args = options.commands;
    std::filesystem::path project_file_path = options.project_file_path;
    luna::authoring::AuthoringPlan plan;

    if (ok && command_args.empty()) {
        reportPlanDiagnostic(state,
                             luna::authoring::AuthoringDiagnosticCode::MissingArgument,
                             "No commands specified.");
        ok = false;
    }

    if (ok && !command_args.empty() && command_args[0] == "plan") {
        if (command_args.size() != 2) {
            reportPlanDiagnostic(state,
                                 luna::authoring::AuthoringDiagnosticCode::MissingArgument,
                                 "Command 'plan' expects exactly one plan path.");
            ok = false;
        } else {
            ok = loadPlan(state, command_args[1], plan);
            if (ok && project_file_path.empty()) {
                project_file_path = plan.project_file_path;
            }
        }
    } else if (ok) {
        std::vector<std::string> parse_errors;
        std::vector<luna::authoring::AuthoringDiagnostic> parse_diagnostics;
        ok = luna::authoring::parseAuthoringCommandTokens(command_args, plan, parse_errors, 0, &parse_diagnostics);
        if (parse_diagnostics.empty()) {
            for (const std::string& error : parse_errors) {
                reportError(state, error);
            }
        } else {
            for (luna::authoring::AuthoringDiagnostic& diagnostic : parse_diagnostics) {
                reportDiagnostic(state, std::move(diagnostic));
            }
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
            reportDiagnostic(state,
                             {
                                 .severity = luna::authoring::AuthoringDiagnosticSeverity::Error,
                                 .phase = luna::authoring::AuthoringDiagnosticPhase::Validate,
                                 .code = luna::authoring::AuthoringDiagnosticCode::ProjectLoadFailed,
                                 .recoverable = true,
                                 .path = project_file_path,
                                 .message = "Failed to load project '" + project_file_path.string() + "'.",
                             });
            ok = false;
        }
    }

    const size_t error_count_before_execute = state.report.errors.size();
    if (ok) {
        luna::authoring::AuthoringExecutor executor(state.session);
        ok = options.dry_run ? executor.validate(plan, state.report) : executor.execute(plan, state.report);
    }

    if (state.json_output) {
        state.report.scene = luna::authoring::captureAuthoringSceneSnapshot(state.session);
        luna::authoring::writeAuthoringReportJson(std::cout, state.report, ok);
    } else if (ok) {
        printWarnings(state.report);
        if (options.dry_run) {
            std::cout << "Dry run: valid\n";
        } else {
            printEntityBindings(state.report);
            printSummary(state);
        }
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
