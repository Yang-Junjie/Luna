#include "AuthoringProtocol.h"

#include "AuthoringSession.h"

#include <charconv>
#include <cstddef>
#include <string_view>
#include <utility>

namespace luna::authoring {
namespace {

bool parseFloat(std::string_view text, float& value)
{
    try {
        size_t parsed = 0;
        const std::string value_text(text);
        value = std::stof(value_text, &parsed);
        return parsed == value_text.size();
    } catch (...) {
        return false;
    }
}

bool parseSize(std::string_view text, size_t& value)
{
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

void addParseDiagnostic(std::vector<std::string>& errors,
                        std::vector<AuthoringDiagnostic>* diagnostics,
                        AuthoringDiagnosticCode code,
                        std::string message,
                        size_t command_index,
                        std::string command = {},
                        std::string field = {},
                        std::string expected = {},
                        std::string actual = {},
                        bool recoverable = true,
                        std::string suggested_command = {})
{
    errors.push_back(message);
    if (diagnostics == nullptr) {
        return;
    }

    diagnostics->push_back({
        .severity = AuthoringDiagnosticSeverity::Error,
        .phase = AuthoringDiagnosticPhase::Parse,
        .code = code,
        .has_command_index = true,
        .command_index = command_index,
        .recoverable = recoverable,
        .command = std::move(command),
        .field = std::move(field),
        .expected = std::move(expected),
        .actual = std::move(actual),
        .suggested_command = std::move(suggested_command),
        .message = std::move(message),
    });
}

bool requireArg(const std::vector<std::string>& tokens,
                size_t index,
                std::string_view command,
                size_t command_index,
                std::vector<std::string>& errors,
                std::vector<AuthoringDiagnostic>* diagnostics)
{
    if (index < tokens.size()) {
        return true;
    }

    addParseDiagnostic(errors,
                       diagnostics,
                       AuthoringDiagnosticCode::MissingArgument,
                       "Missing argument for command '" + std::string(command) + "'.",
                       command_index,
                       std::string(command));
    return false;
}

bool readVec3(const std::vector<std::string>& tokens,
              size_t start,
              std::string_view label,
              std::string_view command,
              size_t command_index,
              glm::vec3& value,
              std::vector<std::string>& errors,
              std::vector<AuthoringDiagnostic>* diagnostics)
{
    float values[3]{};
    for (size_t offset = 0; offset < 3; ++offset) {
        if (!parseFloat(tokens[start + offset], values[offset])) {
            addParseDiagnostic(errors,
                               diagnostics,
                               AuthoringDiagnosticCode::InvalidNumber,
                               "Invalid " + std::string(label) + " number '" + tokens[start + offset] + "'.",
                               command_index,
                               std::string(command),
                               std::string(label));
            return false;
        }
    }

    value = {values[0], values[1], values[2]};
    return true;
}

} // namespace

AuthoringSceneSnapshot captureAuthoringSceneSnapshot(const AuthoringSession& session)
{
    if (!session.hasScene()) {
        return {};
    }

    const Scene& scene = session.scene();
    return {
        .name = scene.getName(),
        .path = session.sceneFilePath(),
        .entity_count = scene.entityManager().entityCount(),
        .dirty = session.isSceneDirty(),
    };
}

AuthoringCommandEffect authoringCommandEffects(AuthoringCommandKind kind)
{
    using Effect = AuthoringCommandEffect;

    switch (kind) {
        case AuthoringCommandKind::NewScene:
        case AuthoringCommandKind::CreateEntity:
        case AuthoringCommandKind::CreateCamera:
        case AuthoringCommandKind::CreateDirectionalLight:
        case AuthoringCommandKind::CreatePointLight:
        case AuthoringCommandKind::CreateSpotLight:
        case AuthoringCommandKind::CreatePrimitive:
        case AuthoringCommandKind::Parent:
        case AuthoringCommandKind::Unparent:
        case AuthoringCommandKind::Rename:
        case AuthoringCommandKind::SetTransform:
        case AuthoringCommandKind::SetLightIntensity:
        case AuthoringCommandKind::SetLightColor:
        case AuthoringCommandKind::SetCameraPerspective:
        case AuthoringCommandKind::SetCameraOrthographic:
            return Effect::MutatesScene;

        case AuthoringCommandKind::OpenScene:
            return Effect::ReadsFileSystem | Effect::MutatesScene;

        case AuthoringCommandKind::SaveScene:
            return Effect::ReadsScene | Effect::MutatesScene | Effect::WritesFileSystem;

        case AuthoringCommandKind::InspectScene:
        case AuthoringCommandKind::InspectEntity:
        case AuthoringCommandKind::InspectHierarchy:
        case AuthoringCommandKind::VerifySceneSaved:
        case AuthoringCommandKind::VerifyEntityExists:
        case AuthoringCommandKind::VerifyHasComponent:
        case AuthoringCommandKind::VerifyEntityCountAtLeast:
            return Effect::ReadsScene;

        case AuthoringCommandKind::Summary:
            return Effect::None;
    }

    return Effect::None;
}

const char* authoringCommandName(AuthoringCommandKind kind)
{
    switch (kind) {
        case AuthoringCommandKind::NewScene:
            return "new";
        case AuthoringCommandKind::OpenScene:
            return "open";
        case AuthoringCommandKind::SaveScene:
            return "save";
        case AuthoringCommandKind::CreateEntity:
            return "entity";
        case AuthoringCommandKind::CreateCamera:
            return "camera";
        case AuthoringCommandKind::CreateDirectionalLight:
            return "directional-light";
        case AuthoringCommandKind::CreatePointLight:
            return "point-light";
        case AuthoringCommandKind::CreateSpotLight:
            return "spot-light";
        case AuthoringCommandKind::CreatePrimitive:
            return "primitive";
        case AuthoringCommandKind::Parent:
            return "parent";
        case AuthoringCommandKind::Unparent:
            return "unparent";
        case AuthoringCommandKind::Rename:
            return "name";
        case AuthoringCommandKind::SetTransform:
            return "transform";
        case AuthoringCommandKind::SetLightIntensity:
            return "light-intensity";
        case AuthoringCommandKind::SetLightColor:
            return "light-color";
        case AuthoringCommandKind::SetCameraPerspective:
            return "camera-perspective";
        case AuthoringCommandKind::SetCameraOrthographic:
            return "camera-orthographic";
        case AuthoringCommandKind::InspectScene:
        case AuthoringCommandKind::InspectEntity:
        case AuthoringCommandKind::InspectHierarchy:
            return "inspect";
        case AuthoringCommandKind::VerifySceneSaved:
        case AuthoringCommandKind::VerifyEntityExists:
        case AuthoringCommandKind::VerifyHasComponent:
        case AuthoringCommandKind::VerifyEntityCountAtLeast:
            return "verify";
        case AuthoringCommandKind::Summary:
            return "summary";
    }

    return "unknown";
}

bool authoringCommandReadsScene(AuthoringCommandKind kind)
{
    return hasAuthoringCommandEffect(authoringCommandEffects(kind), AuthoringCommandEffect::ReadsScene);
}

bool authoringCommandMutatesScene(AuthoringCommandKind kind)
{
    return hasAuthoringCommandEffect(authoringCommandEffects(kind), AuthoringCommandEffect::MutatesScene);
}

bool authoringCommandReadsFileSystem(AuthoringCommandKind kind)
{
    return hasAuthoringCommandEffect(authoringCommandEffects(kind), AuthoringCommandEffect::ReadsFileSystem);
}

bool authoringCommandWritesFileSystem(AuthoringCommandKind kind)
{
    return hasAuthoringCommandEffect(authoringCommandEffects(kind), AuthoringCommandEffect::WritesFileSystem);
}

bool authoringCommandIsReadOnly(AuthoringCommandKind kind)
{
    const AuthoringCommandEffect effects = authoringCommandEffects(kind);
    return !hasAuthoringCommandEffect(effects, AuthoringCommandEffect::MutatesScene) &&
           !hasAuthoringCommandEffect(effects, AuthoringCommandEffect::WritesFileSystem);
}

void appendAuthoringDiagnostic(AuthoringReport& report, AuthoringDiagnostic diagnostic)
{
    if (diagnostic.severity == AuthoringDiagnosticSeverity::Error && !diagnostic.message.empty()) {
        report.errors.push_back(diagnostic.message);
    }
    report.diagnostics.push_back(std::move(diagnostic));
}

bool parseAuthoringCommandTokens(const std::vector<std::string>& tokens,
                                 AuthoringPlan& plan,
                                 std::vector<std::string>& errors,
                                 size_t start_index,
                                 std::vector<AuthoringDiagnostic>* diagnostics)
{
    for (size_t index = start_index; index < tokens.size();) {
        const size_t command_index = plan.commands.size();
        const std::string& command_name = tokens[index++];
        AuthoringCommand command;

        if (command_name == "new") {
            command.kind = AuthoringCommandKind::NewScene;
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "open") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::OpenScene;
            command.path = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "save") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SaveScene;
            command.path = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "entity") {
            if (!requireArg(tokens, index + 1, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateEntity;
            command.alias = tokens[index++];
            command.name = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "camera") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateCamera;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "directional-light") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateDirectionalLight;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "point-light") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreatePointLight;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "spot-light") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateSpotLight;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "primitive") {
            if (!requireArg(tokens, index + 1, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreatePrimitive;
            command.alias = tokens[index++];
            command.mesh = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "parent") {
            if (!requireArg(tokens, index + 1, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::Parent;
            command.child.value = tokens[index++];
            command.parent.value = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "unparent") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::Unparent;
            command.child.value = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "name") {
            if (!requireArg(tokens, index + 1, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::Rename;
            command.entity.value = tokens[index++];
            command.name = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "transform") {
            if (!requireArg(tokens, index + 9, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetTransform;
            command.entity.value = tokens[index++];
            if (!readVec3(tokens, index, "transform", command_name, command_index, command.translation, errors, diagnostics) ||
                !readVec3(tokens, index + 3, "transform", command_name, command_index, command.rotation_degrees, errors, diagnostics) ||
                !readVec3(tokens, index + 6, "transform", command_name, command_index, command.scale, errors, diagnostics)) {
                return false;
            }
            index += 9;
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "light-intensity") {
            if (!requireArg(tokens, index + 1, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetLightIntensity;
            command.entity.value = tokens[index++];
            if (!parseFloat(tokens[index++], command.value)) {
                addParseDiagnostic(errors,
                                   diagnostics,
                                   AuthoringDiagnosticCode::InvalidNumber,
                                   "Invalid light intensity.",
                                   command_index,
                                   command_name,
                                   "value");
                return false;
            }
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "light-color") {
            if (!requireArg(tokens, index + 3, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetLightColor;
            command.entity.value = tokens[index++];
            if (!readVec3(tokens, index, "light color", command_name, command_index, command.color, errors, diagnostics)) {
                return false;
            }
            index += 3;
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "camera-perspective") {
            if (!requireArg(tokens, index + 3, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetCameraPerspective;
            command.entity.value = tokens[index++];
            if (!parseFloat(tokens[index++], command.fov_degrees) ||
                !parseFloat(tokens[index++], command.near_plane) ||
                !parseFloat(tokens[index++], command.far_plane)) {
                addParseDiagnostic(errors,
                                   diagnostics,
                                   AuthoringDiagnosticCode::InvalidNumber,
                                   "Invalid camera perspective arguments.",
                                   command_index,
                                   command_name);
                return false;
            }
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "camera-orthographic") {
            if (!requireArg(tokens, index + 3, command_name, command_index, errors, diagnostics)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetCameraOrthographic;
            command.entity.value = tokens[index++];
            if (!parseFloat(tokens[index++], command.size) ||
                !parseFloat(tokens[index++], command.near_plane) ||
                !parseFloat(tokens[index++], command.far_plane)) {
                addParseDiagnostic(errors,
                                   diagnostics,
                                   AuthoringDiagnosticCode::InvalidNumber,
                                   "Invalid camera orthographic arguments.",
                                   command_index,
                                   command_name);
                return false;
            }
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "inspect") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }

            const std::string& target = tokens[index++];
            if (target == "scene") {
                command.kind = AuthoringCommandKind::InspectScene;
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (target == "hierarchy") {
                command.kind = AuthoringCommandKind::InspectHierarchy;
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (target == "entity") {
                if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::InspectEntity;
                command.entity.value = tokens[index++];
                plan.commands.push_back(std::move(command));
                continue;
            }

            addParseDiagnostic(errors,
                               diagnostics,
                               AuthoringDiagnosticCode::UnsupportedCommand,
                               "Unknown inspect target '" + target + "'.",
                               command_index,
                               command_name,
                               "target");
            return false;
        }

        if (command_name == "verify") {
            if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                return false;
            }

            const std::string& check = tokens[index++];
            if (check == "saved") {
                command.kind = AuthoringCommandKind::VerifySceneSaved;
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (check == "entity") {
                if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::VerifyEntityExists;
                command.entity.value = tokens[index++];
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (check == "component") {
                if (!requireArg(tokens, index + 1, command_name, command_index, errors, diagnostics)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::VerifyHasComponent;
                command.entity.value = tokens[index++];
                command.component = tokens[index++];
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (check == "entity-count-at-least") {
                if (!requireArg(tokens, index, command_name, command_index, errors, diagnostics)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::VerifyEntityCountAtLeast;
                if (!parseSize(tokens[index++], command.count)) {
                    addParseDiagnostic(errors,
                                       diagnostics,
                                       AuthoringDiagnosticCode::InvalidNumber,
                                       "Invalid entity count.",
                                       command_index,
                                       command_name,
                                       "count");
                    return false;
                }
                plan.commands.push_back(std::move(command));
                continue;
            }

            addParseDiagnostic(errors,
                               diagnostics,
                               AuthoringDiagnosticCode::UnsupportedVerifyCheck,
                               "Unknown verify check '" + check + "'.",
                               command_index,
                               command_name,
                               "check");
            return false;
        }

        if (command_name == "summary") {
            command.kind = AuthoringCommandKind::Summary;
            plan.commands.push_back(std::move(command));
            continue;
        }

        addParseDiagnostic(errors,
                           diagnostics,
                           AuthoringDiagnosticCode::UnsupportedCommand,
                           "Unknown command '" + command_name + "'.",
                           command_index,
                           command_name);
        return false;
    }

    return true;
}

} // namespace luna::authoring
