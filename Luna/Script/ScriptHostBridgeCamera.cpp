#include "Scene/Components/CameraComponent.h"
#include "ScriptHostBridgeInternal.h"

namespace {

LunaScriptCameraProjectionType toScriptProjectionType(luna::Camera::ProjectionType type)
{
    switch (type) {
        case luna::Camera::ProjectionType::Perspective:
            return LunaScriptCameraProjectionType_Perspective;
        case luna::Camera::ProjectionType::Orthographic:
            return LunaScriptCameraProjectionType_Orthographic;
        default:
            return LunaScriptCameraProjectionType_Perspective;
    }
}

luna::Camera::ProjectionType toCameraProjectionType(LunaScriptCameraProjectionType type)
{
    switch (type) {
        case LunaScriptCameraProjectionType_Perspective:
            return luna::Camera::ProjectionType::Perspective;
        case LunaScriptCameraProjectionType_Orthographic:
            return luna::Camera::ProjectionType::Orthographic;
        default:
            return luna::Camera::ProjectionType::Perspective;
    }
}

LunaScriptCameraDesc toScriptCameraDesc(const luna::CameraComponent& camera)
{
    LunaScriptCameraDesc desc{};
    desc.primary = camera.primary ? 1 : 0;
    desc.fixed_aspect_ratio = camera.fixedAspectRatio ? 1 : 0;
    desc.projection_type = toScriptProjectionType(camera.projectionType);
    desc.perspective_vertical_fov_radians = camera.perspectiveVerticalFovRadians;
    desc.perspective_near = camera.perspectiveNear;
    desc.perspective_far = camera.perspectiveFar;
    desc.orthographic_size = camera.orthographicSize;
    desc.orthographic_near = camera.orthographicNear;
    desc.orthographic_far = camera.orthographicFar;
    return desc;
}

void applyScriptCameraDesc(luna::CameraComponent& camera, const LunaScriptCameraDesc& desc)
{
    camera.primary = desc.primary != 0;
    camera.fixedAspectRatio = desc.fixed_aspect_ratio != 0;
    camera.projectionType = toCameraProjectionType(desc.projection_type);
    camera.perspectiveVerticalFovRadians = desc.perspective_vertical_fov_radians;
    camera.perspectiveNear = desc.perspective_near;
    camera.perspectiveFar = desc.perspective_far;
    camera.orthographicSize = desc.orthographic_size;
    camera.orthographicNear = desc.orthographic_near;
    camera.orthographicFar = desc.orthographic_far;
}

int entityHasCamera(void* scene_context, uint64_t entity_id)
{
    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    const luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    return entity && entity.hasComponent<luna::CameraComponent>() ? 1 : 0;
}

int entityGetCamera(void* scene_context, uint64_t entity_id, LunaScriptCameraDesc* out_camera)
{
    if (out_camera == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    const luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::CameraComponent>()) {
        return 0;
    }

    *out_camera = toScriptCameraDesc(entity.getComponent<luna::CameraComponent>());
    return 1;
}

int entitySetCamera(void* scene_context, uint64_t entity_id, const LunaScriptCameraDesc* camera)
{
    if (camera == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::CameraComponent>()) {
        return 0;
    }

    applyScriptCameraDesc(entity.getComponent<luna::CameraComponent>(), *camera);
    return 1;
}

int entitySetCameraPrimary(void* scene_context, uint64_t entity_id, int32_t primary)
{
    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::CameraComponent>()) {
        return 0;
    }

    entity.getComponent<luna::CameraComponent>().primary = primary != 0;
    return 1;
}

int entitySetPerspectiveCamera(
    void* scene_context, uint64_t entity_id, float vertical_fov_radians, float near_clip, float far_clip)
{
    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::CameraComponent>()) {
        return 0;
    }

    luna::CameraComponent& camera = entity.getComponent<luna::CameraComponent>();
    camera.projectionType = luna::Camera::ProjectionType::Perspective;
    camera.perspectiveVerticalFovRadians = vertical_fov_radians;
    camera.perspectiveNear = near_clip;
    camera.perspectiveFar = far_clip;
    return 1;
}

int entitySetOrthographicCamera(
    void* scene_context, uint64_t entity_id, float vertical_size, float near_clip, float far_clip)
{
    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::CameraComponent>()) {
        return 0;
    }

    luna::CameraComponent& camera = entity.getComponent<luna::CameraComponent>();
    camera.projectionType = luna::Camera::ProjectionType::Orthographic;
    camera.orthographicSize = vertical_size;
    camera.orthographicNear = near_clip;
    camera.orthographicFar = far_clip;
    return 1;
}

} // namespace

namespace luna {

void registerScriptCameraHostApi(LunaScriptHostApi& host_api)
{
    host_api.entity_has_camera = &entityHasCamera;
    host_api.entity_get_camera = &entityGetCamera;
    host_api.entity_set_camera = &entitySetCamera;
    host_api.entity_set_camera_primary = &entitySetCameraPrimary;
    host_api.entity_set_perspective_camera = &entitySetPerspectiveCamera;
    host_api.entity_set_orthographic_camera = &entitySetOrthographicCamera;
}

} // namespace luna
