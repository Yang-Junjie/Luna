#include "AuthoringValidator.h"

#include "Asset/BuiltinAssets.h"
#include "Authoring/AuthoringSession.h"
#include "Project/ProjectManager.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace luna::authoring {
namespace {

enum class SceneKnowledge : uint8_t {
    CurrentScene,
    GeneratedNewScene,
    OpenedSceneUnknown,
};

enum ComponentBits : uint16_t {
    ComponentId = 1 << 0,
    ComponentTag = 1 << 1,
    ComponentRelationship = 1 << 2,
    ComponentTransform = 1 << 3,
    ComponentCamera = 1 << 4,
    ComponentLight = 1 << 5,
    ComponentMesh = 1 << 6,
    ComponentScript = 1 << 7,
};

struct EntityFacts {
    uint16_t components{0};

    [[nodiscard]] bool has(uint16_t component) const noexcept
    {
        return (components & component) != 0;
    }

    void add(uint16_t component) noexcept
    {
        components |= component;
    }
};

struct ValidationState {
    SceneKnowledge scene_knowledge{SceneKnowledge::CurrentScene};
    std::optional<size_t> entity_count;
    bool scene_saved_known{false};
    std::unordered_map<std::string, EntityFacts> aliases;
    std::unordered_set<std::string> planned_file_writes;
};

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool parseUInt64(std::string_view text, uint64_t& value)
{
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

std::filesystem::path resolveAuthoringPath(const std::filesystem::path& input_path)
{
    if (input_path.empty() || input_path.is_absolute()) {
        return input_path;
    }

    if (const auto project_root = ProjectManager::instance().getProjectRootPath()) {
        return (*project_root / input_path).lexically_normal();
    }

    return input_path.lexically_normal();
}

void addValidationDiagnostic(AuthoringReport& report,
                             AuthoringDiagnosticSeverity severity,
                             AuthoringDiagnosticCode code,
                             std::string message,
                             size_t command_index,
                             const AuthoringCommand& command,
                             std::string entity_ref = {},
                             std::string component = {},
                             std::filesystem::path path = {},
                             std::string field = {})
{
    appendAuthoringDiagnostic(report,
                              {
                                  .severity = severity,
                                  .phase = AuthoringDiagnosticPhase::Validate,
                                  .code = code,
                                  .has_command_index = true,
                                  .command_index = command_index,
                                  .command = authoringCommandName(command.kind),
                                  .field = std::move(field),
                                  .entity_ref = std::move(entity_ref),
                                  .component = std::move(component),
                                  .path = std::move(path),
                                  .message = std::move(message),
                              });
}

void addPlanDiagnostic(AuthoringReport& report, AuthoringDiagnosticCode code, std::string message)
{
    appendAuthoringDiagnostic(report,
                              {
                                  .severity = AuthoringDiagnosticSeverity::Error,
                                  .phase = AuthoringDiagnosticPhase::Validate,
                                  .code = code,
                                  .message = std::move(message),
                              });
}

EntityFacts transformEntityFacts()
{
    return {
        .components = ComponentId | ComponentTag | ComponentRelationship | ComponentTransform,
    };
}

EntityFacts cameraEntityFacts()
{
    EntityFacts facts = transformEntityFacts();
    facts.add(ComponentCamera);
    return facts;
}

EntityFacts lightEntityFacts()
{
    EntityFacts facts = transformEntityFacts();
    facts.add(ComponentLight);
    return facts;
}

EntityFacts meshEntityFacts()
{
    EntityFacts facts = transformEntityFacts();
    facts.add(ComponentMesh);
    return facts;
}

EntityFacts factsFromSceneEntity(const Scene& scene, UUID uuid)
{
    EntityFacts facts{};
    const auto entity_handle = scene.entityManager().findEntityHandleByUUID(uuid);
    if (!entity_handle.has_value()) {
        return facts;
    }

    const auto& registry = scene.entityManager().registry();
    if (registry.all_of<IDComponent>(*entity_handle)) {
        facts.add(ComponentId);
    }
    if (registry.all_of<TagComponent>(*entity_handle)) {
        facts.add(ComponentTag);
    }
    if (registry.all_of<RelationshipComponent>(*entity_handle)) {
        facts.add(ComponentRelationship);
    }
    if (registry.all_of<TransformComponent>(*entity_handle)) {
        facts.add(ComponentTransform);
    }
    if (registry.all_of<CameraComponent>(*entity_handle)) {
        facts.add(ComponentCamera);
    }
    if (registry.all_of<LightComponent>(*entity_handle)) {
        facts.add(ComponentLight);
    }
    if (registry.all_of<MeshComponent>(*entity_handle)) {
        facts.add(ComponentMesh);
    }
    if (registry.all_of<ScriptComponent>(*entity_handle)) {
        facts.add(ComponentScript);
    }
    return facts;
}

std::optional<uint16_t> componentBitByName(std::string_view component_name)
{
    const std::string normalized_name = toLower(std::string(component_name));
    if (normalized_name == "id") {
        return ComponentId;
    }
    if (normalized_name == "tag") {
        return ComponentTag;
    }
    if (normalized_name == "relationship") {
        return ComponentRelationship;
    }
    if (normalized_name == "transform") {
        return ComponentTransform;
    }
    if (normalized_name == "camera") {
        return ComponentCamera;
    }
    if (normalized_name == "light") {
        return ComponentLight;
    }
    if (normalized_name == "mesh") {
        return ComponentMesh;
    }
    if (normalized_name == "script") {
        return ComponentScript;
    }

    return std::nullopt;
}

bool findBuiltinMesh(std::string_view mesh_name)
{
    const std::string requested_name = toLower(std::string(mesh_name));
    for (const auto& mesh : BuiltinAssets::getBuiltinMeshes()) {
        if (toLower(mesh.Name) == requested_name) {
            return true;
        }
    }

    return false;
}

std::optional<EntityFacts> resolveEntityFacts(const AuthoringCommand& command,
                                              std::string_view reference,
                                              const AuthoringSession& session,
                                              const ValidationState& state,
                                              AuthoringReport& report,
                                              size_t command_index)
{
    if (reference.empty()) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::InvalidArgument,
                                "Entity reference cannot be empty.",
                                command_index,
                                command,
                                std::string(reference),
                                {},
                                {},
                                "entity");
        return std::nullopt;
    }

    if (const auto alias_it = state.aliases.find(std::string(reference)); alias_it != state.aliases.end()) {
        return alias_it->second;
    }

    uint64_t uuid = 0;
    if (!parseUInt64(reference, uuid)) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::UnknownEntity,
                                "Unknown entity reference '" + std::string(reference) + "'.",
                                command_index,
                                command,
                                std::string(reference));
        return std::nullopt;
    }

    if (state.scene_knowledge == SceneKnowledge::OpenedSceneUnknown) {
        return std::nullopt;
    }

    if (state.scene_knowledge == SceneKnowledge::CurrentScene && session.hasScene()) {
        const EntityFacts facts = factsFromSceneEntity(session.scene(), UUID(uuid));
        if (facts.components != 0) {
            return facts;
        }
    }

    addValidationDiagnostic(report,
                            AuthoringDiagnosticSeverity::Error,
                            AuthoringDiagnosticCode::UnknownEntity,
                            "Unknown entity reference '" + std::string(reference) + "'.",
                            command_index,
                            command,
                            std::string(reference));
    return std::nullopt;
}

bool requireEntityFacts(const AuthoringCommand& command,
                        std::string_view reference,
                        const AuthoringSession& session,
                        const ValidationState& state,
                        AuthoringReport& report,
                        size_t command_index)
{
    const size_t errors_before = report.errors.size();
    (void) resolveEntityFacts(command, reference, session, state, report, command_index);
    return report.errors.size() == errors_before;
}

bool requireComponent(const AuthoringCommand& command,
                      std::string_view reference,
                      std::string_view component_name,
                      const AuthoringSession& session,
                      const ValidationState& state,
                      AuthoringReport& report,
                      size_t command_index)
{
    const size_t errors_before = report.errors.size();
    const std::optional<EntityFacts> facts =
        resolveEntityFacts(command, reference, session, state, report, command_index);
    if (report.errors.size() != errors_before) {
        return false;
    }

    const std::optional<uint16_t> component_bit = componentBitByName(component_name);
    if (!component_bit.has_value()) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::MissingComponent,
                                "Unknown component '" + std::string(component_name) + "'.",
                                command_index,
                                command,
                                std::string(reference),
                                std::string(component_name));
        return false;
    }

    if (facts.has_value() && !facts->has(*component_bit)) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::MissingComponent,
                                "Entity '" + std::string(reference) + "' does not have component '" +
                                    std::string(component_name) + "'.",
                                command_index,
                                command,
                                std::string(reference),
                                std::string(component_name));
        return false;
    }

    return true;
}

bool validateOpenPath(const AuthoringCommand& command,
                      AuthoringReport& report,
                      size_t command_index,
                      const AuthoringValidationOptions& options)
{
    const std::filesystem::path scene_path = resolveAuthoringPath(command.path);
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_path);
    if (normalized_scene_path.empty()) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::OpenSceneFailed,
                                "Open scene path cannot be empty.",
                                command_index,
                                command,
                                {},
                                {},
                                normalized_scene_path,
                                "path");
        return false;
    }

    if (!options.check_file_system) {
        return true;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(normalized_scene_path, ec);
    if (ec || !exists) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::OpenSceneFailed,
                                "Scene file does not exist: '" + normalized_scene_path.string() + "'.",
                                command_index,
                                command,
                                {},
                                {},
                                normalized_scene_path,
                                "path");
        return false;
    }

    const bool regular_file = std::filesystem::is_regular_file(normalized_scene_path, ec);
    if (ec || !regular_file) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::OpenSceneFailed,
                                "Scene path is not a file: '" + normalized_scene_path.string() + "'.",
                                command_index,
                                command,
                                {},
                                {},
                                normalized_scene_path,
                                "path");
        return false;
    }

    return true;
}

bool validateSavePath(const AuthoringCommand& command,
                      ValidationState& state,
                      AuthoringReport& report,
                      size_t command_index,
                      const AuthoringValidationOptions& options)
{
    const std::filesystem::path scene_path = resolveAuthoringPath(command.path);
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_path);
    if (normalized_scene_path.empty()) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::SaveSceneFailed,
                                "Save scene path cannot be empty.",
                                command_index,
                                command,
                                {},
                                {},
                                normalized_scene_path,
                                "path");
        return false;
    }

    const std::string write_key = normalized_scene_path.lexically_normal().string();
    if (!state.planned_file_writes.insert(write_key).second) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Warning,
                                AuthoringDiagnosticCode::FileOverwrite,
                                "Plan saves the same scene path more than once: '" + normalized_scene_path.string() +
                                    "'.",
                                command_index,
                                command,
                                {},
                                {},
                                normalized_scene_path,
                                "path");
    }

    if (!options.check_file_system) {
        return true;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(normalized_scene_path, ec);
    if (ec) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::SaveSceneFailed,
                                "Failed to inspect scene file '" + normalized_scene_path.string() + "': " +
                                    ec.message() + ".",
                                command_index,
                                command,
                                {},
                                {},
                                normalized_scene_path,
                                "path");
        return false;
    }

    if (exists) {
        const bool regular_file = std::filesystem::is_regular_file(normalized_scene_path, ec);
        if (ec || !regular_file) {
            addValidationDiagnostic(report,
                                    AuthoringDiagnosticSeverity::Error,
                                    AuthoringDiagnosticCode::SaveSceneFailed,
                                    "Refusing to overwrite non-file scene path '" + normalized_scene_path.string() +
                                        "'.",
                                    command_index,
                                    command,
                                    {},
                                    {},
                                    normalized_scene_path,
                                    "path");
            return false;
        }

        if (options.warn_on_file_overwrite) {
            addValidationDiagnostic(report,
                                    AuthoringDiagnosticSeverity::Warning,
                                    AuthoringDiagnosticCode::FileOverwrite,
                                    "Save will overwrite existing scene file '" + normalized_scene_path.string() + "'.",
                                    command_index,
                                    command,
                                    {},
                                    {},
                                    normalized_scene_path,
                                    "path");
        }
    }

    const std::filesystem::path parent_path = normalized_scene_path.parent_path();
    if (parent_path.empty()) {
        return true;
    }

    const bool parent_exists = std::filesystem::exists(parent_path, ec);
    if (ec) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::SaveSceneFailed,
                                "Failed to inspect scene directory '" + parent_path.string() + "': " + ec.message() +
                                    ".",
                                command_index,
                                command,
                                {},
                                {},
                                parent_path,
                                "path");
        return false;
    }

    if (parent_exists) {
        const bool parent_is_directory = std::filesystem::is_directory(parent_path, ec);
        if (ec || !parent_is_directory) {
            addValidationDiagnostic(report,
                                    AuthoringDiagnosticSeverity::Error,
                                    AuthoringDiagnosticCode::SaveSceneFailed,
                                    "Scene parent path is not a directory: '" + parent_path.string() + "'.",
                                    command_index,
                                    command,
                                    {},
                                    {},
                                    parent_path,
                                    "path");
            return false;
        }
        return true;
    }

    for (std::filesystem::path probe = parent_path; !probe.empty();) {
        const bool probe_exists = std::filesystem::exists(probe, ec);
        if (ec) {
            return true;
        }
        if (probe_exists) {
            const bool probe_is_directory = std::filesystem::is_directory(probe, ec);
            if (!ec && !probe_is_directory) {
                addValidationDiagnostic(report,
                                        AuthoringDiagnosticSeverity::Error,
                                        AuthoringDiagnosticCode::SaveSceneFailed,
                                        "Scene parent ancestor is not a directory: '" + probe.string() + "'.",
                                        command_index,
                                        command,
                                        {},
                                        {},
                                        probe,
                                        "path");
                return false;
            }
            return true;
        }

        const std::filesystem::path next = probe.parent_path();
        if (next == probe) {
            break;
        }
        probe = next;
    }

    return true;
}

bool validateAliasAvailable(const AuthoringCommand& command,
                            ValidationState& state,
                            EntityFacts facts,
                            AuthoringReport& report,
                            size_t command_index)
{
    if (command.alias.empty()) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::InvalidArgument,
                                "Entity alias cannot be empty.",
                                command_index,
                                command,
                                command.alias,
                                {},
                                {},
                                "alias");
        return false;
    }

    if (state.aliases.contains(command.alias)) {
        addValidationDiagnostic(report,
                                AuthoringDiagnosticSeverity::Error,
                                AuthoringDiagnosticCode::InvalidArgument,
                                "Entity alias '" + command.alias + "' is already used in this plan.",
                                command_index,
                                command,
                                command.alias,
                                {},
                                {},
                                "alias");
        return false;
    }

    state.aliases.emplace(command.alias, facts);
    if (state.entity_count.has_value()) {
        ++(*state.entity_count);
    }
    state.scene_saved_known = false;
    return true;
}

void markSceneMutated(ValidationState& state)
{
    state.scene_saved_known = false;
}

} // namespace

bool validateAuthoringPlan(const AuthoringPlan& plan,
                           const AuthoringSession& session,
                           AuthoringReport& report,
                           const AuthoringValidationOptions& options)
{
    const size_t errors_before = report.errors.size();
    report.scene = captureAuthoringSceneSnapshot(session);

    if (plan.protocol.name != kAuthoringProtocolName ||
        plan.protocol.version != kAuthoringProtocolVersion) {
        addPlanDiagnostic(report,
                          AuthoringDiagnosticCode::ProtocolMismatch,
                          "Unsupported authoring protocol '" + plan.protocol.name + "' version '" +
                              std::to_string(plan.protocol.version) + "'.");
        return false;
    }

    if (!session.hasScene()) {
        addPlanDiagnostic(report, AuthoringDiagnosticCode::NoBoundScene, "Authoring session has no bound scene.");
        return false;
    }

    if (plan.commands.empty()) {
        addPlanDiagnostic(report, AuthoringDiagnosticCode::InvalidPlan, "Authoring plan has no commands.");
        return false;
    }

    ValidationState state;
    state.entity_count = session.scene().entityManager().entityCount();
    state.scene_saved_known = !session.sceneFilePath().empty() && !session.isSceneDirty();

    for (size_t command_index = 0; command_index < plan.commands.size(); ++command_index) {
        const AuthoringCommand& command = plan.commands[command_index];

        switch (command.kind) {
            case AuthoringCommandKind::NewScene:
                state.aliases.clear();
                state.scene_knowledge = SceneKnowledge::GeneratedNewScene;
                state.entity_count = 2;
                state.scene_saved_known = false;
                break;

            case AuthoringCommandKind::OpenScene:
                if (validateOpenPath(command, report, command_index, options)) {
                    state.aliases.clear();
                    state.scene_knowledge = SceneKnowledge::OpenedSceneUnknown;
                    state.entity_count.reset();
                    state.scene_saved_known = true;
                }
                break;

            case AuthoringCommandKind::SaveScene:
                if (validateSavePath(command, state, report, command_index, options)) {
                    state.scene_saved_known = true;
                }
                break;

            case AuthoringCommandKind::CreateEntity:
                (void) validateAliasAvailable(command, state, transformEntityFacts(), report, command_index);
                break;

            case AuthoringCommandKind::CreateCamera:
                (void) validateAliasAvailable(command, state, cameraEntityFacts(), report, command_index);
                break;

            case AuthoringCommandKind::CreateDirectionalLight:
            case AuthoringCommandKind::CreatePointLight:
            case AuthoringCommandKind::CreateSpotLight:
                (void) validateAliasAvailable(command, state, lightEntityFacts(), report, command_index);
                break;

            case AuthoringCommandKind::CreatePrimitive:
                if (command.mesh.empty() || !findBuiltinMesh(command.mesh)) {
                    addValidationDiagnostic(report,
                                            AuthoringDiagnosticSeverity::Error,
                                            AuthoringDiagnosticCode::UnknownBuiltinAsset,
                                            "Unknown builtin mesh '" + command.mesh + "'.",
                                            command_index,
                                            command,
                                            {},
                                            {},
                                            {},
                                            "mesh");
                    break;
                }
                (void) validateAliasAvailable(command, state, meshEntityFacts(), report, command_index);
                break;

            case AuthoringCommandKind::Parent:
                if (requireEntityFacts(command, command.child.value, session, state, report, command_index) &&
                    requireEntityFacts(command, command.parent.value, session, state, report, command_index)) {
                    markSceneMutated(state);
                }
                break;

            case AuthoringCommandKind::Unparent:
                if (requireEntityFacts(command, command.child.value, session, state, report, command_index)) {
                    markSceneMutated(state);
                }
                break;

            case AuthoringCommandKind::Rename:
                if (command.name.empty()) {
                    addValidationDiagnostic(report,
                                            AuthoringDiagnosticSeverity::Error,
                                            AuthoringDiagnosticCode::InvalidArgument,
                                            "Entity name cannot be empty.",
                                            command_index,
                                            command,
                                            command.entity.value,
                                            {},
                                            {},
                                            "name");
                    break;
                }
                if (requireEntityFacts(command, command.entity.value, session, state, report, command_index)) {
                    markSceneMutated(state);
                }
                break;

            case AuthoringCommandKind::SetTransform:
                if (requireComponent(command,
                                     command.entity.value,
                                     "Transform",
                                     session,
                                     state,
                                     report,
                                     command_index)) {
                    markSceneMutated(state);
                }
                break;

            case AuthoringCommandKind::SetLightIntensity:
            case AuthoringCommandKind::SetLightColor:
                if (requireComponent(command,
                                     command.entity.value,
                                     "Light",
                                     session,
                                     state,
                                     report,
                                     command_index)) {
                    markSceneMutated(state);
                }
                break;

            case AuthoringCommandKind::SetCameraPerspective:
                if (command.fov_degrees <= 0.0f || command.fov_degrees >= 180.0f ||
                    command.near_plane <= 0.0f || command.far_plane <= command.near_plane) {
                    addValidationDiagnostic(report,
                                            AuthoringDiagnosticSeverity::Error,
                                            AuthoringDiagnosticCode::InvalidArgument,
                                            "Camera perspective requires 0 < fovDeg < 180 and 0 < near < far.",
                                            command_index,
                                            command,
                                            command.entity.value);
                    break;
                }
                if (const size_t errors_before = report.errors.size();
                    (resolveEntityFacts(command,
                                        command.entity.value,
                                        session,
                                        state,
                                        report,
                                        command_index)
                         .has_value() ||
                     state.scene_knowledge == SceneKnowledge::OpenedSceneUnknown) &&
                    report.errors.size() == errors_before) {
                    if (auto alias_it = state.aliases.find(command.entity.value); alias_it != state.aliases.end()) {
                        alias_it->second.add(ComponentCamera);
                    }
                    markSceneMutated(state);
                }
                break;

            case AuthoringCommandKind::SetCameraOrthographic:
                if (command.size <= 0.0f || command.far_plane <= command.near_plane) {
                    addValidationDiagnostic(report,
                                            AuthoringDiagnosticSeverity::Error,
                                            AuthoringDiagnosticCode::InvalidArgument,
                                            "Camera orthographic projection requires size > 0 and near < far.",
                                            command_index,
                                            command,
                                            command.entity.value);
                    break;
                }
                if (const size_t errors_before = report.errors.size();
                    (resolveEntityFacts(command,
                                        command.entity.value,
                                        session,
                                        state,
                                        report,
                                        command_index)
                         .has_value() ||
                     state.scene_knowledge == SceneKnowledge::OpenedSceneUnknown) &&
                    report.errors.size() == errors_before) {
                    if (auto alias_it = state.aliases.find(command.entity.value); alias_it != state.aliases.end()) {
                        alias_it->second.add(ComponentCamera);
                    }
                    markSceneMutated(state);
                }
                break;

            case AuthoringCommandKind::InspectScene:
            case AuthoringCommandKind::InspectHierarchy:
                break;

            case AuthoringCommandKind::InspectEntity:
                (void) requireEntityFacts(command, command.entity.value, session, state, report, command_index);
                break;

            case AuthoringCommandKind::VerifySceneSaved:
                if (!state.scene_saved_known) {
                    addValidationDiagnostic(report,
                                            AuthoringDiagnosticSeverity::Error,
                                            AuthoringDiagnosticCode::VerificationFailed,
                                            "Verification failed: Scene has no known saved state.",
                                            command_index,
                                            command);
                }
                break;

            case AuthoringCommandKind::VerifyEntityExists:
                (void) requireEntityFacts(command, command.entity.value, session, state, report, command_index);
                break;

            case AuthoringCommandKind::VerifyHasComponent:
                (void) requireComponent(command,
                                        command.entity.value,
                                        command.component,
                                        session,
                                        state,
                                        report,
                                        command_index);
                break;

            case AuthoringCommandKind::VerifyEntityCountAtLeast:
                if (state.entity_count.has_value() && *state.entity_count < command.count) {
                    addValidationDiagnostic(report,
                                            AuthoringDiagnosticSeverity::Error,
                                            AuthoringDiagnosticCode::VerificationFailed,
                                            "Verification failed: Scene entity count is below minimum.",
                                            command_index,
                                            command);
                }
                break;

            case AuthoringCommandKind::Summary:
                break;
        }
    }

    return report.errors.size() == errors_before;
}

} // namespace luna::authoring
