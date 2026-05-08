#include "AuthoringExecutor.h"

#include "Asset/BuiltinAssets.h"
#include "Authoring/AuthoringInspection.h"
#include "Authoring/AuthoringSession.h"
#include "Project/ProjectManager.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace luna::authoring {
namespace {

class AuthoringTransactionGuard {
public:
    AuthoringTransactionGuard(AuthoringSession& session, AuthoringReport& report, bool owns_transaction)
        : m_session(session),
          m_report(report),
          m_owns_transaction(owns_transaction)
    {}

    AuthoringTransactionGuard(const AuthoringTransactionGuard&) = delete;
    AuthoringTransactionGuard& operator=(const AuthoringTransactionGuard&) = delete;

    ~AuthoringTransactionGuard()
    {
        if (m_owns_transaction && !m_committed) {
            (void) m_session.rollbackTransaction();
            m_report.entities.clear();
            m_report.saved_scenes.clear();
            m_report.scene = captureAuthoringSceneSnapshot(m_session);
        }
    }

    void commit()
    {
        if (!m_owns_transaction || m_committed) {
            return;
        }

        (void) m_session.commitTransaction();
        m_committed = true;
    }

private:
    AuthoringSession& m_session;
    AuthoringReport& m_report;
    bool m_owns_transaction{false};
    bool m_committed{false};
};

class AuthoringFileEffectGuard {
public:
    AuthoringFileEffectGuard() = default;

    AuthoringFileEffectGuard(const AuthoringFileEffectGuard&) = delete;
    AuthoringFileEffectGuard& operator=(const AuthoringFileEffectGuard&) = delete;

    ~AuthoringFileEffectGuard()
    {
        if (!m_committed) {
            rollback();
        }
    }

    bool capture(const std::filesystem::path& path, std::string& error_message)
    {
        if (path.empty() || hasCapture(path)) {
            return true;
        }

        FileSnapshot snapshot;
        snapshot.path = path;

        std::error_code ec;
        snapshot.existed = std::filesystem::exists(path, ec);
        if (ec) {
            error_message = "Failed to inspect scene file '" + path.string() + "': " + ec.message() + ".";
            return false;
        }

        if (snapshot.existed) {
            snapshot.was_regular_file = std::filesystem::is_regular_file(path, ec);
            if (ec) {
                error_message = "Failed to inspect scene file '" + path.string() + "': " + ec.message() + ".";
                return false;
            }
            if (!snapshot.was_regular_file) {
                error_message = "Refusing to overwrite non-file scene path '" + path.string() + "'.";
                return false;
            }

            std::ifstream input_stream(path, std::ios::binary);
            if (!input_stream.is_open()) {
                error_message = "Failed to snapshot existing scene file '" + path.string() + "'.";
                return false;
            }

            snapshot.contents.assign(std::istreambuf_iterator<char>(input_stream),
                                     std::istreambuf_iterator<char>());
            if (input_stream.bad()) {
                error_message = "Failed to read existing scene file '" + path.string() + "'.";
                return false;
            }
        }

        m_snapshots.push_back(std::move(snapshot));
        return true;
    }

    void commit() noexcept
    {
        m_committed = true;
    }

private:
    struct FileSnapshot {
        std::filesystem::path path;
        bool existed{false};
        bool was_regular_file{false};
        std::string contents;
    };

    bool hasCapture(const std::filesystem::path& path) const
    {
        return std::any_of(m_snapshots.begin(), m_snapshots.end(), [&path](const FileSnapshot& snapshot) {
            return snapshot.path == path;
        });
    }

    void rollback() noexcept
    {
        for (auto snapshot_it = m_snapshots.rbegin(); snapshot_it != m_snapshots.rend(); ++snapshot_it) {
            const FileSnapshot& snapshot = *snapshot_it;
            std::error_code ec;
            if (snapshot.existed) {
                if (!snapshot.path.parent_path().empty()) {
                    std::filesystem::create_directories(snapshot.path.parent_path(), ec);
                    if (ec) {
                        continue;
                    }
                }

                std::ofstream output_stream(snapshot.path, std::ios::binary | std::ios::trunc);
                if (!output_stream.is_open()) {
                    continue;
                }
                output_stream << snapshot.contents;
            } else {
                std::filesystem::remove(snapshot.path, ec);
            }
        }
    }

    std::vector<FileSnapshot> m_snapshots;
    bool m_committed{false};
};

const char* commandKindName(AuthoringCommandKind kind)
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

void addDiagnostic(AuthoringReport& report,
                   AuthoringDiagnosticCode code,
                   std::string message,
                   size_t command_index,
                   const AuthoringCommand& command,
                   AuthoringDiagnosticPhase phase = AuthoringDiagnosticPhase::Execute,
                   std::string entity_ref = {},
                   std::string component = {},
                   std::filesystem::path path = {},
                   std::string field = {})
{
    appendAuthoringDiagnostic(report,
                              {
                                  .severity = AuthoringDiagnosticSeverity::Error,
                                  .phase = phase,
                                  .code = code,
                                  .has_command_index = true,
                                  .command_index = command_index,
                                  .command = commandKindName(command.kind),
                                  .field = std::move(field),
                                  .entity_ref = std::move(entity_ref),
                                  .component = std::move(component),
                                  .path = std::move(path),
                                  .message = std::move(message),
                              });
}

bool failVerification(AuthoringReport& report,
                      AuthoringVerification verification,
                      AuthoringDiagnosticCode code,
                      size_t command_index,
                      const AuthoringCommand& command)
{
    addDiagnostic(report,
                  code,
                  "Verification failed: " + verification.message,
                  command_index,
                  command,
                  AuthoringDiagnosticPhase::Verify,
                  verification.ref,
                  verification.component);
    report.verifications.push_back(std::move(verification));
    return false;
}

bool passVerification(AuthoringReport& report, AuthoringVerification verification)
{
    verification.ok = true;
    report.verifications.push_back(std::move(verification));
    return true;
}

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

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameTransformComponent(const TransformComponent& lhs, const TransformComponent& rhs)
{
    return sameVec3(lhs.translation, rhs.translation) &&
           sameVec3(lhs.rotation, rhs.rotation) &&
           sameVec3(lhs.scale, rhs.scale);
}

bool sameLightComponent(const LightComponent& lhs, const LightComponent& rhs)
{
    return lhs.type == rhs.type &&
           lhs.enabled == rhs.enabled &&
           sameVec3(lhs.color, rhs.color) &&
           lhs.intensity == rhs.intensity &&
           lhs.range == rhs.range &&
           lhs.innerConeAngleRadians == rhs.innerConeAngleRadians &&
           lhs.outerConeAngleRadians == rhs.outerConeAngleRadians;
}

bool sameCameraComponent(const CameraComponent& lhs, const CameraComponent& rhs)
{
    return lhs.primary == rhs.primary &&
           lhs.fixedAspectRatio == rhs.fixedAspectRatio &&
           lhs.projectionType == rhs.projectionType &&
           lhs.perspectiveVerticalFovRadians == rhs.perspectiveVerticalFovRadians &&
           lhs.perspectiveNear == rhs.perspectiveNear &&
           lhs.perspectiveFar == rhs.perspectiveFar &&
           lhs.orthographicSize == rhs.orthographicSize &&
           lhs.orthographicNear == rhs.orthographicNear &&
           lhs.orthographicFar == rhs.orthographicFar;
}

bool hasComponentByName(Entity entity, std::string_view component_name)
{
    const std::string normalized_name = toLower(std::string(component_name));
    if (normalized_name == "id") {
        return entity.hasComponent<IDComponent>();
    }
    if (normalized_name == "tag") {
        return entity.hasComponent<TagComponent>();
    }
    if (normalized_name == "relationship") {
        return entity.hasComponent<RelationshipComponent>();
    }
    if (normalized_name == "transform") {
        return entity.hasComponent<TransformComponent>();
    }
    if (normalized_name == "camera") {
        return entity.hasComponent<CameraComponent>();
    }
    if (normalized_name == "light") {
        return entity.hasComponent<LightComponent>();
    }
    if (normalized_name == "mesh") {
        return entity.hasComponent<MeshComponent>();
    }
    if (normalized_name == "script") {
        return entity.hasComponent<ScriptComponent>();
    }

    return false;
}

std::optional<AssetHandle> findBuiltinMesh(std::string_view mesh_name)
{
    const std::string requested_name = toLower(std::string(mesh_name));
    for (const auto& mesh : BuiltinAssets::getBuiltinMeshes()) {
        if (toLower(mesh.Name) == requested_name) {
            return mesh.Handle;
        }
    }

    return std::nullopt;
}

std::filesystem::path resolveScenePath(const std::filesystem::path& input_path)
{
    if (input_path.empty() || input_path.is_absolute()) {
        return input_path;
    }

    if (const auto project_root = ProjectManager::instance().getProjectRootPath()) {
        return (*project_root / input_path).lexically_normal();
    }

    return input_path;
}

bool prepareCommandFileWrites(AuthoringFileEffectGuard& file_effect_guard,
                              const AuthoringCommand& command,
                              AuthoringReport& report,
                              size_t command_index)
{
    if (!authoringCommandWritesFileSystem(command.kind)) {
        return true;
    }

    switch (command.kind) {
        case AuthoringCommandKind::SaveScene: {
            const std::filesystem::path scene_path = resolveScenePath(command.path);
            const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_path);
            std::string file_snapshot_error;
            if (!file_effect_guard.capture(normalized_scene_path, file_snapshot_error)) {
                addDiagnostic(report,
                              AuthoringDiagnosticCode::SaveSceneFailed,
                              file_snapshot_error,
                              command_index,
                              command,
                              AuthoringDiagnosticPhase::Execute,
                              {},
                              {},
                              normalized_scene_path);
                return false;
            }
            return true;
        }

        default:
            addDiagnostic(report,
                          AuthoringDiagnosticCode::ExecutionFailed,
                          "Filesystem-writing command '" + std::string(commandKindName(command.kind)) +
                              "' has no authoring rollback boundary.",
                          command_index,
                          command);
            return false;
    }
}

} // namespace

AuthoringExecutor::AuthoringExecutor(AuthoringSession& session)
    : m_session(session)
{}

void AuthoringExecutor::clearAliases()
{
    m_aliases.clear();
}

Entity AuthoringExecutor::resolveEntity(std::string_view reference) const
{
    if (!m_session.hasScene()) {
        return {};
    }

    if (const auto alias_it = m_aliases.find(std::string(reference)); alias_it != m_aliases.end()) {
        return m_session.scene().entityManager().findEntityByUUID(alias_it->second);
    }

    uint64_t uuid = 0;
    if (!parseUInt64(reference, uuid)) {
        return {};
    }

    return m_session.scene().entityManager().findEntityByUUID(UUID(uuid));
}

bool AuthoringExecutor::requireEntity(std::string_view reference,
                                      Entity& entity,
                                      AuthoringReport& report,
                                      size_t command_index,
                                      const AuthoringCommand& command) const
{
    entity = resolveEntity(reference);
    if (entity) {
        return true;
    }

    addDiagnostic(report,
                  AuthoringDiagnosticCode::UnknownEntity,
                  "Unknown entity reference '" + std::string(reference) + "'.",
                  command_index,
                  command,
                  AuthoringDiagnosticPhase::Execute,
                  std::string(reference));
    return false;
}

bool AuthoringExecutor::rememberAlias(const std::string& alias,
                                      Entity entity,
                                      AuthoringReport& report,
                                      size_t command_index,
                                      const AuthoringCommand& command)
{
    if (alias.empty()) {
        addDiagnostic(report,
                      AuthoringDiagnosticCode::InvalidArgument,
                      "Entity alias cannot be empty.",
                      command_index,
                      command,
                      AuthoringDiagnosticPhase::Execute,
                      alias);
        return false;
    }

    if (!entity) {
        addDiagnostic(report,
                      AuthoringDiagnosticCode::ExecutionFailed,
                      "Command did not create an entity for alias '" + alias + "'.",
                      command_index,
                      command,
                      AuthoringDiagnosticPhase::Execute,
                      alias);
        return false;
    }

    m_aliases[alias] = entity.getUUID();
    report.entities.push_back({
        .alias = alias,
        .entity_id = entity.getUUID(),
        .name = entity.getName(),
    });
    return true;
}

void AuthoringExecutor::refreshEntityBindingName(Entity entity, AuthoringReport& report) const
{
    if (!entity) {
        return;
    }

    const UUID entity_id = entity.getUUID();
    for (AuthoringEntityBinding& binding : report.entities) {
        if (binding.entity_id == entity_id) {
            binding.name = entity.getName();
        }
    }
}

bool AuthoringExecutor::execute(const AuthoringPlan& plan, AuthoringReport& report)
{
    if (!m_session.hasScene()) {
        appendAuthoringDiagnostic(report,
                                  {
                                      .severity = AuthoringDiagnosticSeverity::Error,
                                      .phase = AuthoringDiagnosticPhase::Execute,
                                      .code = AuthoringDiagnosticCode::NoBoundScene,
                                      .message = "Authoring executor has no bound scene.",
                                  });
        report.scene = captureAuthoringSceneSnapshot(m_session);
        return false;
    }

    AuthoringFileEffectGuard file_effect_guard;
    AuthoringTransactionGuard transaction_guard(m_session, report, m_session.beginTransaction("Authoring Plan"));

    for (size_t command_index = 0; command_index < plan.commands.size(); ++command_index) {
        const AuthoringCommand& command = plan.commands[command_index];
        if (!prepareCommandFileWrites(file_effect_guard, command, report, command_index)) {
            report.scene = captureAuthoringSceneSnapshot(m_session);
            return false;
        }

        switch (command.kind) {
            case AuthoringCommandKind::NewScene:
                (void) m_session.createScene();
                clearAliases();
                report.entities.clear();
                break;

            case AuthoringCommandKind::OpenScene: {
                const std::filesystem::path scene_path = resolveScenePath(command.path);
                if (!m_session.openScene(scene_path)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::OpenSceneFailed,
                                  "Failed to open scene '" + scene_path.string() + "'.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  {},
                                  {},
                                  scene_path);
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                clearAliases();
                report.entities.clear();
                break;
            }

            case AuthoringCommandKind::SaveScene: {
                const std::filesystem::path scene_path = resolveScenePath(command.path);
                if (!m_session.saveSceneAs(scene_path)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::SaveSceneFailed,
                                  "Failed to save scene '" + scene_path.string() + "'.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  {},
                                  {},
                                  scene_path);
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                report.saved_scenes.push_back(m_session.sceneFilePath());
                break;
            }

            case AuthoringCommandKind::CreateEntity:
                if (!rememberAlias(command.alias, m_session.createEntity(command.name), report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreateCamera:
                if (!rememberAlias(command.alias, m_session.createCameraEntity(), report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreateDirectionalLight:
                if (!rememberAlias(command.alias,
                                   m_session.createDirectionalLightEntity(),
                                   report,
                                   command_index,
                                   command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreatePointLight:
                if (!rememberAlias(command.alias, m_session.createPointLightEntity(), report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreateSpotLight:
                if (!rememberAlias(command.alias, m_session.createSpotLightEntity(), report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreatePrimitive: {
                const std::optional<AssetHandle> mesh_handle = findBuiltinMesh(command.mesh);
                if (!mesh_handle) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::UnknownBuiltinAsset,
                                  "Unknown builtin mesh '" + command.mesh + "'.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  {},
                                  {},
                                  {},
                                  "mesh");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!rememberAlias(command.alias,
                                   m_session.createPrimitiveEntity(*mesh_handle),
                                   report,
                                   command_index,
                                   command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::Parent: {
                Entity child;
                Entity parent;
                if (!requireEntity(command.child.value, child, report, command_index, command) ||
                    !requireEntity(command.parent.value, parent, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!m_session.reparentEntity(child, parent, true)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to parent entity.",
                                  command_index,
                                  command);
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::Unparent: {
                Entity child;
                if (!requireEntity(command.child.value, child, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!m_session.reparentEntity(child, {}, true)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to unparent entity.",
                                  command_index,
                                  command);
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::Rename: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (entity.getName() != command.name && !m_session.setEntityName(entity, command.name)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to rename entity.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value);
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                refreshEntityBindingName(entity, report);
                break;
            }

            case AuthoringCommandKind::SetTransform: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!entity.hasComponent<TransformComponent>()) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::MissingComponent,
                                  "Entity does not have a Transform component.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value,
                                  "Transform");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                TransformComponent transform = entity.transform();
                transform.translation = command.translation;
                transform.rotation = glm::radians(command.rotation_degrees);
                transform.scale = command.scale;
                if (!sameTransformComponent(entity.transform(), transform) &&
                    !m_session.setEntityTransform(entity, transform)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to set transform.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value);
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetLightIntensity: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!entity.hasComponent<LightComponent>()) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::MissingComponent,
                                  "Entity does not have a Light component.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value,
                                  "Light");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                LightComponent light = entity.getComponent<LightComponent>();
                light.intensity = command.value;
                if (!sameLightComponent(entity.getComponent<LightComponent>(), light) &&
                    !m_session.setLightComponent(entity, light)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to set light intensity.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value,
                                  "Light");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetLightColor: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!entity.hasComponent<LightComponent>()) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::MissingComponent,
                                  "Entity does not have a Light component.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value,
                                  "Light");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                LightComponent light = entity.getComponent<LightComponent>();
                light.color = command.color;
                if (!sameLightComponent(entity.getComponent<LightComponent>(), light) &&
                    !m_session.setLightComponent(entity, light)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to set light color.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value,
                                  "Light");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetCameraPerspective: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                const bool had_camera = entity.hasComponent<CameraComponent>();
                CameraComponent camera = had_camera ? entity.getComponent<CameraComponent>() : CameraComponent{};
                camera.projectionType = Camera::ProjectionType::Perspective;
                camera.perspectiveVerticalFovRadians = glm::radians(command.fov_degrees);
                camera.perspectiveNear = command.near_plane;
                camera.perspectiveFar = command.far_plane;
                if (!(had_camera && sameCameraComponent(entity.getComponent<CameraComponent>(), camera)) &&
                    !m_session.setCameraComponent(entity, camera)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to set camera perspective.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value,
                                  "Camera");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetCameraOrthographic: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                const bool had_camera = entity.hasComponent<CameraComponent>();
                CameraComponent camera = had_camera ? entity.getComponent<CameraComponent>() : CameraComponent{};
                camera.projectionType = Camera::ProjectionType::Orthographic;
                camera.orthographicSize = command.size;
                camera.orthographicNear = command.near_plane;
                camera.orthographicFar = command.far_plane;
                if (!(had_camera && sameCameraComponent(entity.getComponent<CameraComponent>(), camera)) &&
                    !m_session.setCameraComponent(entity, camera)) {
                    addDiagnostic(report,
                                  AuthoringDiagnosticCode::ExecutionFailed,
                                  "Failed to set camera orthographic projection.",
                                  command_index,
                                  command,
                                  AuthoringDiagnosticPhase::Execute,
                                  command.entity.value,
                                  "Camera");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::InspectScene:
                report.inspections.push_back(inspectAuthoringScene(m_session));
                break;

            case AuthoringCommandKind::InspectEntity: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report, command_index, command)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                report.inspections.push_back(inspectAuthoringEntityResult(entity, command.entity.value));
                break;
            }

            case AuthoringCommandKind::InspectHierarchy:
                report.inspections.push_back(inspectAuthoringHierarchy(m_session));
                break;

            case AuthoringCommandKind::VerifySceneSaved: {
                AuthoringVerification verification{
                    .kind = AuthoringVerificationKind::SceneSaved,
                    .message = "Scene is saved.",
                };

                if (m_session.sceneFilePath().empty()) {
                    verification.message = "Scene has no save path.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report,
                                            std::move(verification),
                                            AuthoringDiagnosticCode::VerificationFailed,
                                            command_index,
                                            command);
                }
                if (m_session.isSceneDirty()) {
                    verification.message = "Scene has unsaved changes.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report,
                                            std::move(verification),
                                            AuthoringDiagnosticCode::VerificationFailed,
                                            command_index,
                                            command);
                }

                (void) passVerification(report, std::move(verification));
                break;
            }

            case AuthoringCommandKind::VerifyEntityExists: {
                AuthoringVerification verification{
                    .kind = AuthoringVerificationKind::EntityExists,
                    .ref = command.entity.value,
                    .message = "Entity exists.",
                };

                Entity entity = resolveEntity(command.entity.value);
                if (!entity) {
                    verification.message = "Unknown entity reference '" + command.entity.value + "'.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report,
                                            std::move(verification),
                                            AuthoringDiagnosticCode::UnknownEntity,
                                            command_index,
                                            command);
                }

                verification.entity_id = entity.getUUID();
                (void) passVerification(report, std::move(verification));
                break;
            }

            case AuthoringCommandKind::VerifyHasComponent: {
                AuthoringVerification verification{
                    .kind = AuthoringVerificationKind::HasComponent,
                    .ref = command.entity.value,
                    .component = command.component,
                    .message = "Entity has component.",
                };

                Entity entity = resolveEntity(command.entity.value);
                if (!entity) {
                    verification.message = "Unknown entity reference '" + command.entity.value + "'.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report,
                                            std::move(verification),
                                            AuthoringDiagnosticCode::UnknownEntity,
                                            command_index,
                                            command);
                }

                verification.entity_id = entity.getUUID();
                if (!hasComponentByName(entity, command.component)) {
                    verification.message =
                        "Entity '" + command.entity.value + "' does not have component '" + command.component + "'.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report,
                                            std::move(verification),
                                            AuthoringDiagnosticCode::MissingComponent,
                                            command_index,
                                            command);
                }

                (void) passVerification(report, std::move(verification));
                break;
            }

            case AuthoringCommandKind::VerifyEntityCountAtLeast: {
                const size_t actual_count = m_session.scene().entityManager().entityCount();
                AuthoringVerification verification{
                    .kind = AuthoringVerificationKind::EntityCountAtLeast,
                    .expected_count = command.count,
                    .actual_count = actual_count,
                    .message = "Scene entity count meets minimum.",
                };

                if (actual_count < command.count) {
                    verification.message = "Scene entity count is below minimum.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report,
                                            std::move(verification),
                                            AuthoringDiagnosticCode::VerificationFailed,
                                            command_index,
                                            command);
                }

                (void) passVerification(report, std::move(verification));
                break;
            }

            case AuthoringCommandKind::Summary:
                break;
        }
    }

    transaction_guard.commit();
    file_effect_guard.commit();
    report.scene = captureAuthoringSceneSnapshot(m_session);
    return true;
}

} // namespace luna::authoring
