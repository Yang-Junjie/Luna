#include "Authoring/AuthoringSession.h"
#include "AuthoringInspection.h"
#include "Renderer/Camera.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <glm/trigonometric.hpp>
#include <utility>

namespace luna::authoring {
namespace {

const char* lightTypeName(LightComponent::Type type)
{
    switch (type) {
        case LightComponent::Type::Directional:
            return "Directional";
        case LightComponent::Type::Point:
            return "Point";
        case LightComponent::Type::Spot:
            return "Spot";
    }

    return "Unknown";
}

const char* projectionTypeName(Camera::ProjectionType type)
{
    switch (type) {
        case Camera::ProjectionType::Perspective:
            return "Perspective";
        case Camera::ProjectionType::Orthographic:
            return "Orthographic";
    }

    return "Unknown";
}

} // namespace

AuthoringEntityInspection inspectAuthoringEntity(Entity entity, std::string ref)
{
    AuthoringEntityInspection inspection;
    inspection.ref = std::move(ref);
    if (!entity) {
        return inspection;
    }

    inspection.entity_id = entity.getUUID();
    inspection.name = entity.getName();
    inspection.parent_id = entity.getParentUUID();
    inspection.children = entity.getChildren();

    if (entity.hasComponent<IDComponent>()) {
        inspection.components.emplace_back("ID");
    }
    if (entity.hasComponent<TagComponent>()) {
        inspection.components.emplace_back("Tag");
    }
    if (entity.hasComponent<RelationshipComponent>()) {
        inspection.components.emplace_back("Relationship");
    }
    if (entity.hasComponent<TransformComponent>()) {
        inspection.components.emplace_back("Transform");
        inspection.has_transform = true;
        const TransformComponent& transform = entity.transform();
        inspection.transform.translation = transform.translation;
        inspection.transform.rotation_degrees = glm::degrees(transform.rotation);
        inspection.transform.scale = transform.scale;
    }
    if (entity.hasComponent<CameraComponent>()) {
        inspection.components.emplace_back("Camera");
        inspection.has_camera = true;
        const CameraComponent& camera = entity.getComponent<CameraComponent>();
        inspection.camera.primary = camera.primary;
        inspection.camera.fixed_aspect_ratio = camera.fixedAspectRatio;
        inspection.camera.projection = projectionTypeName(camera.projectionType);
        inspection.camera.perspective_fov_degrees = glm::degrees(camera.perspectiveVerticalFovRadians);
        inspection.camera.perspective_near = camera.perspectiveNear;
        inspection.camera.perspective_far = camera.perspectiveFar;
        inspection.camera.orthographic_size = camera.orthographicSize;
        inspection.camera.orthographic_near = camera.orthographicNear;
        inspection.camera.orthographic_far = camera.orthographicFar;
    }
    if (entity.hasComponent<LightComponent>()) {
        inspection.components.emplace_back("Light");
        inspection.has_light = true;
        const LightComponent& light = entity.getComponent<LightComponent>();
        inspection.light.type = lightTypeName(light.type);
        inspection.light.enabled = light.enabled;
        inspection.light.color = light.color;
        inspection.light.intensity = light.intensity;
        inspection.light.range = light.range;
        inspection.light.inner_cone_angle_degrees = glm::degrees(light.innerConeAngleRadians);
        inspection.light.outer_cone_angle_degrees = glm::degrees(light.outerConeAngleRadians);
    }
    if (entity.hasComponent<MeshComponent>()) {
        inspection.components.emplace_back("Mesh");
        inspection.has_mesh = true;
        const MeshComponent& mesh = entity.getComponent<MeshComponent>();
        inspection.mesh.mesh_handle = mesh.meshHandle;
        inspection.mesh.first_submesh = mesh.firstSubmesh;
        inspection.mesh.submesh_count = mesh.submeshCount;
        inspection.mesh.submesh_materials = mesh.submeshMaterials;
    }
    if (entity.hasComponent<ScriptComponent>()) {
        inspection.components.emplace_back("Script");
    }

    return inspection;
}

AuthoringInspection inspectAuthoringScene(AuthoringSession& session)
{
    AuthoringInspection inspection;
    inspection.kind = AuthoringInspectionKind::Scene;
    inspection.ref = "scene";
    if (!session.hasScene()) {
        return inspection;
    }

    Scene& scene = session.scene();
    auto view = scene.entityManager().registry().view<const IDComponent>();
    inspection.entities.reserve(scene.entityManager().entityCount());
    for (const auto entity_handle : view) {
        inspection.entities.push_back(inspectAuthoringEntity(Entity(entity_handle, &scene.entityManager())));
    }

    return inspection;
}

AuthoringInspection inspectAuthoringEntityResult(Entity entity, std::string ref)
{
    AuthoringInspection inspection;
    inspection.kind = AuthoringInspectionKind::Entity;
    inspection.ref = ref;
    if (entity) {
        inspection.entities.push_back(inspectAuthoringEntity(entity, std::move(ref)));
    }
    return inspection;
}

AuthoringInspection inspectAuthoringHierarchy(AuthoringSession& session)
{
    AuthoringInspection inspection = inspectAuthoringScene(session);
    inspection.kind = AuthoringInspectionKind::Hierarchy;
    inspection.ref = "hierarchy";
    return inspection;
}

} // namespace luna::authoring
