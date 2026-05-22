#include "Viewport/PreviewSceneViewport.h"

#include "Asset/BuiltinAssets.h"
#include "Renderer/RenderWorld/RenderWorldExtractor.h"
#include "Renderer/Renderer.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Viewport/SceneViewportInstance.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace {

glm::vec3 toGlmVec3(luna::editor::Vec3 value) noexcept
{
    return glm::vec3{value.x, value.y, value.z};
}

luna::SceneBackgroundMode toSceneBackgroundMode(luna::editor::SceneViewportPreviewBackground background)
{
    switch (background) {
        case luna::editor::SceneViewportPreviewBackground::SolidColor:
            return luna::SceneBackgroundMode::SolidColor;
        case luna::editor::SceneViewportPreviewBackground::EnvironmentMap:
            return luna::SceneBackgroundMode::EnvironmentMap;
        case luna::editor::SceneViewportPreviewBackground::ProceduralSky:
            return luna::SceneBackgroundMode::ProceduralSky;
    }

    return luna::SceneBackgroundMode::ProceduralSky;
}

luna::SceneEnvironmentSettings toSceneEnvironmentSettings(
    const luna::editor::SceneViewportPreviewEnvironment& environment)
{
    luna::SceneEnvironmentSettings settings{};
    settings.backgroundMode = toSceneBackgroundMode(environment.background);
    settings.backgroundColor = toGlmVec3(environment.background_color);
    settings.enabled = settings.backgroundMode != luna::SceneBackgroundMode::SolidColor;
    settings.iblEnabled = environment.ibl_enabled;
    settings.environmentMapHandle = environment.environment_map;
    settings.intensity = (std::max)(environment.intensity, 0.0f);
    settings.skyIntensity = (std::max)(environment.sky_intensity, 0.0f);
    settings.diffuseIntensity = (std::max)(environment.diffuse_intensity, 0.0f);
    settings.specularIntensity = (std::max)(environment.specular_intensity, 0.0f);
    const glm::vec3 sun_direction = toGlmVec3(environment.procedural_sun_direction);
    settings.proceduralSunDirection =
        glm::dot(sun_direction, sun_direction) > 0.0001f ? glm::normalize(sun_direction) : glm::vec3{0.4f, 0.6f, 0.3f};
    settings.proceduralSunIntensity = (std::max)(environment.procedural_sun_intensity, 0.0f);
    settings.proceduralSunAngularRadius = (std::max)(environment.procedural_sun_angular_radius, 0.0f);
    settings.proceduralSkyColorZenith = toGlmVec3(environment.procedural_sky_color_zenith);
    settings.proceduralSkyColorHorizon = toGlmVec3(environment.procedural_sky_color_horizon);
    settings.proceduralGroundColor = toGlmVec3(environment.procedural_ground_color);
    settings.proceduralSkyExposure = (std::max)(environment.procedural_sky_exposure, 0.0f);
    return settings;
}

bool sameVec3(luna::editor::Vec3 lhs, luna::editor::Vec3 rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameEnvironment(const luna::editor::SceneViewportPreviewEnvironment& lhs,
                     const luna::editor::SceneViewportPreviewEnvironment& rhs) noexcept
{
    return lhs.background == rhs.background && sameVec3(lhs.background_color, rhs.background_color) &&
           lhs.ibl_enabled == rhs.ibl_enabled && lhs.environment_map == rhs.environment_map &&
           lhs.intensity == rhs.intensity && lhs.sky_intensity == rhs.sky_intensity &&
           lhs.diffuse_intensity == rhs.diffuse_intensity && lhs.specular_intensity == rhs.specular_intensity &&
           sameVec3(lhs.procedural_sun_direction, rhs.procedural_sun_direction) &&
           lhs.procedural_sun_intensity == rhs.procedural_sun_intensity &&
           lhs.procedural_sun_angular_radius == rhs.procedural_sun_angular_radius &&
           sameVec3(lhs.procedural_sky_color_zenith, rhs.procedural_sky_color_zenith) &&
           sameVec3(lhs.procedural_sky_color_horizon, rhs.procedural_sky_color_horizon) &&
           sameVec3(lhs.procedural_ground_color, rhs.procedural_ground_color) &&
           lhs.procedural_sky_exposure == rhs.procedural_sky_exposure;
}

} // namespace

namespace luna {

const editor::SceneViewportPreviewState& PreviewSceneViewport::state() const noexcept
{
    return m_state;
}

bool PreviewSceneViewport::dirty() const noexcept
{
    return m_dirty;
}

bool PreviewSceneViewport::setState(const editor::SceneViewportPreviewState& state)
{
    if (!m_dirty && sameState(m_state, state)) {
        m_state = state;
        return false;
    }

    m_state = state;
    m_dirty = true;
    return true;
}

void PreviewSceneViewport::sync(Renderer& renderer, SceneViewportInstance& viewport)
{
    if (m_dirty) {
        rebuildScene();
    } else {
        applyCamera(m_camera, m_state);
    }

    RenderWorldExtractor{}.extract(m_scene,
                                   m_camera,
                                   renderer.getSceneViewportRenderWorld(viewport.rendererViewportHandle(renderer)));
}

AssetHandle PreviewSceneViewport::previewMeshHandle(const editor::SceneViewportPreviewState& state)
{
    if (state.mesh.isValid()) {
        return state.mesh;
    }

    switch (state.mesh_kind) {
        case editor::SceneViewportPreviewMesh::Cube:
            return BuiltinMeshes::Cube;
        case editor::SceneViewportPreviewMesh::Plane:
            return BuiltinMeshes::Plane;
        case editor::SceneViewportPreviewMesh::Sphere:
            return BuiltinMeshes::Sphere;
    }

    return BuiltinMeshes::Sphere;
}

void PreviewSceneViewport::rebuildScene()
{
    Scene& scene = m_scene;
    scene.entityManager().clear();
    scene.setName("Editor Preview");
    scene.setAssetLoadBehavior(Scene::AssetLoadBehavior::NonBlocking);
    scene.environmentSettings() = toSceneEnvironmentSettings(m_state.environment);
    scene.shadowSettings().mode = SceneShadowMode::PcfShadowMap;
    scene.shadowSettings().pcfShadowDistance = 12.0f;
    scene.shadowSettings().pcfMapSize = 1024;
    scene.shadowSettings().csmCascadeSize = 1024;

    Entity material_entity = scene.entityManager().createEntity("Preview Mesh");
    material_entity.transform().translation = glm::vec3{0.0f, 0.0f, 0.0f};
    material_entity.transform().rotation = glm::vec3{0.0f, glm::radians(25.0f), 0.0f};
    material_entity.transform().scale = glm::vec3{1.0f};
    auto& mesh_component = material_entity.addComponent<MeshComponent>();
    mesh_component.meshHandle = previewMeshHandle(m_state);
    if (m_state.material.isValid()) {
        mesh_component.setSubmeshMaterial(0, m_state.material);
    }

    Entity key_light = scene.entityManager().createEntity("Preview Key Light");
    key_light.transform().rotation = glm::vec3{glm::radians(-45.0f), glm::radians(35.0f), 0.0f};
    auto& light_component = key_light.addComponent<LightComponent>();
    light_component.type = LightComponent::Type::Directional;
    light_component.intensity = 3.0f;
    light_component.color = glm::vec3{1.0f, 0.96f, 0.90f};

    applyCamera(m_camera, m_state);
    m_dirty = false;
}

void PreviewSceneViewport::applyCamera(Camera& camera, const editor::SceneViewportPreviewState& state)
{
    const glm::vec3 position = state.override_camera ? toGlmVec3(state.camera_position) : glm::vec3{0.0f, 0.35f, 3.1f};
    const glm::vec3 target = state.override_camera ? toGlmVec3(state.camera_target) : glm::vec3{0.0f, 0.0f, 0.0f};
    const float fov_degrees = state.override_camera ? state.camera_vertical_fov_degrees : 45.0f;
    const float near_clip = state.override_camera ? state.camera_near_clip : 0.05f;
    const float far_clip = state.override_camera ? state.camera_far_clip : 100.0f;

    camera.setPerspective(glm::radians(std::clamp(fov_degrees, 1.0f, 120.0f)), near_clip, far_clip);
    camera.setPosition(position);
    camera.lookAt(target);
}

bool PreviewSceneViewport::sameState(const editor::SceneViewportPreviewState& lhs,
                                     const editor::SceneViewportPreviewState& rhs) noexcept
{
    return lhs.material == rhs.material && lhs.mesh == rhs.mesh && lhs.mesh_kind == rhs.mesh_kind &&
           sameEnvironment(lhs.environment, rhs.environment);
}

} // namespace luna
