#include "AuthoringExecutor.h"

#include "Asset/BuiltinAssets.h"
#include "Authoring/AuthoringInspection.h"
#include "Authoring/AuthoringSession.h"
#include "Project/ProjectManager.h"
#include "Scene/Components.h"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace luna::authoring {
namespace {

void addError(AuthoringReport& report, std::string message)
{
    report.errors.push_back(std::move(message));
}

bool failVerification(AuthoringReport& report, AuthoringVerification verification)
{
    report.errors.push_back("Verification failed: " + verification.message);
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

bool AuthoringExecutor::requireEntity(std::string_view reference, Entity& entity, AuthoringReport& report) const
{
    entity = resolveEntity(reference);
    if (entity) {
        return true;
    }

    addError(report, "Unknown entity reference '" + std::string(reference) + "'.");
    return false;
}

bool AuthoringExecutor::rememberAlias(const std::string& alias, Entity entity, AuthoringReport& report)
{
    if (alias.empty()) {
        addError(report, "Entity alias cannot be empty.");
        return false;
    }

    if (!entity) {
        addError(report, "Command did not create an entity for alias '" + alias + "'.");
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
        addError(report, "Authoring executor has no bound scene.");
        report.scene = captureAuthoringSceneSnapshot(m_session);
        return false;
    }

    for (const AuthoringCommand& command : plan.commands) {
        switch (command.kind) {
            case AuthoringCommandKind::NewScene:
                (void) m_session.createScene();
                clearAliases();
                report.entities.clear();
                break;

            case AuthoringCommandKind::OpenScene: {
                const std::filesystem::path scene_path = resolveScenePath(command.path);
                if (!m_session.openScene(scene_path)) {
                    addError(report, "Failed to open scene '" + scene_path.string() + "'.");
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
                    addError(report, "Failed to save scene '" + scene_path.string() + "'.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                report.saved_scenes.push_back(m_session.sceneFilePath());
                break;
            }

            case AuthoringCommandKind::CreateEntity:
                if (!rememberAlias(command.alias, m_session.createEntity(command.name), report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreateCamera:
                if (!rememberAlias(command.alias, m_session.createCameraEntity(), report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreateDirectionalLight:
                if (!rememberAlias(command.alias, m_session.createDirectionalLightEntity(), report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreatePointLight:
                if (!rememberAlias(command.alias, m_session.createPointLightEntity(), report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreateSpotLight:
                if (!rememberAlias(command.alias, m_session.createSpotLightEntity(), report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;

            case AuthoringCommandKind::CreatePrimitive: {
                const std::optional<AssetHandle> mesh_handle = findBuiltinMesh(command.mesh);
                if (!mesh_handle) {
                    addError(report, "Unknown builtin mesh '" + command.mesh + "'.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!rememberAlias(command.alias, m_session.createPrimitiveEntity(*mesh_handle), report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::Parent: {
                Entity child;
                Entity parent;
                if (!requireEntity(command.child.value, child, report) ||
                    !requireEntity(command.parent.value, parent, report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!m_session.reparentEntity(child, parent, true)) {
                    addError(report, "Failed to parent entity.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::Unparent: {
                Entity child;
                if (!requireEntity(command.child.value, child, report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!m_session.reparentEntity(child, {}, true)) {
                    addError(report, "Failed to unparent entity.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::Rename: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (entity.getName() != command.name && !m_session.setEntityName(entity, command.name)) {
                    addError(report, "Failed to rename entity.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                refreshEntityBindingName(entity, report);
                break;
            }

            case AuthoringCommandKind::SetTransform: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!entity.hasComponent<TransformComponent>()) {
                    addError(report, "Entity does not have a Transform component.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                TransformComponent transform = entity.transform();
                transform.translation = command.translation;
                transform.rotation = glm::radians(command.rotation_degrees);
                transform.scale = command.scale;
                if (!sameTransformComponent(entity.transform(), transform) &&
                    !m_session.setEntityTransform(entity, transform)) {
                    addError(report, "Failed to set transform.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetLightIntensity: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!entity.hasComponent<LightComponent>()) {
                    addError(report, "Entity does not have a Light component.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                LightComponent light = entity.getComponent<LightComponent>();
                light.intensity = command.value;
                if (!sameLightComponent(entity.getComponent<LightComponent>(), light) &&
                    !m_session.setLightComponent(entity, light)) {
                    addError(report, "Failed to set light intensity.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetLightColor: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report)) {
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                if (!entity.hasComponent<LightComponent>()) {
                    addError(report, "Entity does not have a Light component.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                LightComponent light = entity.getComponent<LightComponent>();
                light.color = command.color;
                if (!sameLightComponent(entity.getComponent<LightComponent>(), light) &&
                    !m_session.setLightComponent(entity, light)) {
                    addError(report, "Failed to set light color.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetCameraPerspective: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report)) {
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
                    addError(report, "Failed to set camera perspective.");
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return false;
                }
                break;
            }

            case AuthoringCommandKind::SetCameraOrthographic: {
                Entity entity;
                if (!requireEntity(command.entity.value, entity, report)) {
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
                    addError(report, "Failed to set camera orthographic projection.");
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
                if (!requireEntity(command.entity.value, entity, report)) {
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
                    return failVerification(report, std::move(verification));
                }
                if (m_session.isSceneDirty()) {
                    verification.message = "Scene has unsaved changes.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report, std::move(verification));
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
                    return failVerification(report, std::move(verification));
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
                    return failVerification(report, std::move(verification));
                }

                verification.entity_id = entity.getUUID();
                if (!hasComponentByName(entity, command.component)) {
                    verification.message =
                        "Entity '" + command.entity.value + "' does not have component '" + command.component + "'.";
                    report.scene = captureAuthoringSceneSnapshot(m_session);
                    return failVerification(report, std::move(verification));
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
                    return failVerification(report, std::move(verification));
                }

                (void) passVerification(report, std::move(verification));
                break;
            }

            case AuthoringCommandKind::Summary:
                break;
        }
    }

    report.scene = captureAuthoringSceneSnapshot(m_session);
    return true;
}

} // namespace luna::authoring
