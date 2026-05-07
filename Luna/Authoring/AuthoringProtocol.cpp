#include "AuthoringProtocol.h"

#include "AuthoringSession.h"

#include <charconv>
#include <cstddef>
#include <string_view>

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

bool requireArg(const std::vector<std::string>& tokens,
                size_t index,
                std::string_view command,
                std::vector<std::string>& errors)
{
    if (index < tokens.size()) {
        return true;
    }

    errors.push_back("Missing argument for command '" + std::string(command) + "'.");
    return false;
}

bool readVec3(const std::vector<std::string>& tokens,
              size_t start,
              std::string_view label,
              glm::vec3& value,
              std::vector<std::string>& errors)
{
    float values[3]{};
    for (size_t offset = 0; offset < 3; ++offset) {
        if (!parseFloat(tokens[start + offset], values[offset])) {
            errors.push_back("Invalid " + std::string(label) + " number '" + tokens[start + offset] + "'.");
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

bool parseAuthoringCommandTokens(const std::vector<std::string>& tokens,
                                 AuthoringPlan& plan,
                                 std::vector<std::string>& errors,
                                 size_t start_index)
{
    for (size_t index = start_index; index < tokens.size();) {
        const std::string& command_name = tokens[index++];
        AuthoringCommand command;

        if (command_name == "new") {
            command.kind = AuthoringCommandKind::NewScene;
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "open") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::OpenScene;
            command.path = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "save") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SaveScene;
            command.path = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "entity") {
            if (!requireArg(tokens, index + 1, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateEntity;
            command.alias = tokens[index++];
            command.name = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "camera") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateCamera;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "directional-light") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateDirectionalLight;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "point-light") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreatePointLight;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "spot-light") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreateSpotLight;
            command.alias = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "primitive") {
            if (!requireArg(tokens, index + 1, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::CreatePrimitive;
            command.alias = tokens[index++];
            command.mesh = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "parent") {
            if (!requireArg(tokens, index + 1, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::Parent;
            command.child.value = tokens[index++];
            command.parent.value = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "unparent") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::Unparent;
            command.child.value = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "name") {
            if (!requireArg(tokens, index + 1, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::Rename;
            command.entity.value = tokens[index++];
            command.name = tokens[index++];
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "transform") {
            if (!requireArg(tokens, index + 9, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetTransform;
            command.entity.value = tokens[index++];
            if (!readVec3(tokens, index, "transform", command.translation, errors) ||
                !readVec3(tokens, index + 3, "transform", command.rotation_degrees, errors) ||
                !readVec3(tokens, index + 6, "transform", command.scale, errors)) {
                return false;
            }
            index += 9;
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "light-intensity") {
            if (!requireArg(tokens, index + 1, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetLightIntensity;
            command.entity.value = tokens[index++];
            if (!parseFloat(tokens[index++], command.value)) {
                errors.emplace_back("Invalid light intensity.");
                return false;
            }
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "light-color") {
            if (!requireArg(tokens, index + 3, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetLightColor;
            command.entity.value = tokens[index++];
            if (!readVec3(tokens, index, "light color", command.color, errors)) {
                return false;
            }
            index += 3;
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "camera-perspective") {
            if (!requireArg(tokens, index + 3, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetCameraPerspective;
            command.entity.value = tokens[index++];
            if (!parseFloat(tokens[index++], command.fov_degrees) ||
                !parseFloat(tokens[index++], command.near_plane) ||
                !parseFloat(tokens[index++], command.far_plane)) {
                errors.emplace_back("Invalid camera perspective arguments.");
                return false;
            }
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "camera-orthographic") {
            if (!requireArg(tokens, index + 3, command_name, errors)) {
                return false;
            }
            command.kind = AuthoringCommandKind::SetCameraOrthographic;
            command.entity.value = tokens[index++];
            if (!parseFloat(tokens[index++], command.size) ||
                !parseFloat(tokens[index++], command.near_plane) ||
                !parseFloat(tokens[index++], command.far_plane)) {
                errors.emplace_back("Invalid camera orthographic arguments.");
                return false;
            }
            plan.commands.push_back(std::move(command));
            continue;
        }

        if (command_name == "inspect") {
            if (!requireArg(tokens, index, command_name, errors)) {
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
                if (!requireArg(tokens, index, command_name, errors)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::InspectEntity;
                command.entity.value = tokens[index++];
                plan.commands.push_back(std::move(command));
                continue;
            }

            errors.push_back("Unknown inspect target '" + target + "'.");
            return false;
        }

        if (command_name == "verify") {
            if (!requireArg(tokens, index, command_name, errors)) {
                return false;
            }

            const std::string& check = tokens[index++];
            if (check == "saved") {
                command.kind = AuthoringCommandKind::VerifySceneSaved;
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (check == "entity") {
                if (!requireArg(tokens, index, command_name, errors)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::VerifyEntityExists;
                command.entity.value = tokens[index++];
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (check == "component") {
                if (!requireArg(tokens, index + 1, command_name, errors)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::VerifyHasComponent;
                command.entity.value = tokens[index++];
                command.component = tokens[index++];
                plan.commands.push_back(std::move(command));
                continue;
            }
            if (check == "entity-count-at-least") {
                if (!requireArg(tokens, index, command_name, errors)) {
                    return false;
                }
                command.kind = AuthoringCommandKind::VerifyEntityCountAtLeast;
                if (!parseSize(tokens[index++], command.count)) {
                    errors.emplace_back("Invalid entity count.");
                    return false;
                }
                plan.commands.push_back(std::move(command));
                continue;
            }

            errors.push_back("Unknown verify check '" + check + "'.");
            return false;
        }

        if (command_name == "summary") {
            command.kind = AuthoringCommandKind::Summary;
            plan.commands.push_back(std::move(command));
            continue;
        }

        errors.push_back("Unknown command '" + command_name + "'.");
        return false;
    }

    return true;
}

} // namespace luna::authoring
