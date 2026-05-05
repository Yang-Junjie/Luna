#include "SceneSerializer.h"

#include "Core/Log.h"
#include "Entity.h"
#include "Scene.h"

#include <algorithm>
#include <fstream>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace {

struct SerializedEntityData {
    uint64_t id = 0;
    std::string tag = "Entity";
    luna::TransformComponent transform;
    uint64_t parent_id = 0;
    std::vector<uint64_t> serialized_children;
    bool has_camera_component = false;
    luna::CameraComponent camera;
    bool has_light_component = false;
    luna::LightComponent light;
    bool has_mesh_component = false;
    luna::MeshComponent mesh;
    bool has_script_component = false;
    luna::ScriptComponent script;
};

void emitVec3(YAML::Emitter& out, const glm::vec3& value)
{
    out << YAML::Flow;
    out << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
}

glm::vec3 readVec3(const YAML::Node& node, const glm::vec3& fallback)
{
    if (!node || !node.IsSequence() || node.size() != 3) {
        return fallback;
    }

    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>()};
}

const char* sceneBackgroundModeToString(luna::SceneBackgroundMode mode)
{
    switch (mode) {
        case luna::SceneBackgroundMode::SolidColor:
            return "SolidColor";
        case luna::SceneBackgroundMode::ProceduralSky:
            return "ProceduralSky";
        case luna::SceneBackgroundMode::EnvironmentMap:
            return "EnvironmentMap";
    }

    return "ProceduralSky";
}

const char* sceneShadowModeToString(luna::SceneShadowMode mode)
{
    switch (mode) {
        case luna::SceneShadowMode::None:
            return "None";
        case luna::SceneShadowMode::PcfShadowMap:
            return "PcfShadowMap";
        case luna::SceneShadowMode::CascadedShadowMaps:
            return "CascadedShadowMaps";
    }

    return "CascadedShadowMaps";
}

luna::SceneBackgroundMode readSceneBackgroundMode(const YAML::Node& node, luna::SceneBackgroundMode fallback)
{
    if (!node) {
        return fallback;
    }

    const std::string mode = node.as<std::string>();
    if (mode == "SolidColor") {
        return luna::SceneBackgroundMode::SolidColor;
    }
    if (mode == "ProceduralSky" || mode == "DefaultSky") {
        return luna::SceneBackgroundMode::ProceduralSky;
    }
    if (mode == "EnvironmentMap") {
        return luna::SceneBackgroundMode::EnvironmentMap;
    }

    return fallback;
}

luna::SceneShadowMode readSceneShadowMode(const YAML::Node& node, luna::SceneShadowMode fallback)
{
    if (!node) {
        return fallback;
    }

    const std::string mode = node.as<std::string>();
    if (mode == "None") {
        return luna::SceneShadowMode::None;
    }
    if (mode == "PcfShadowMap" || mode == "PCF" || mode == "PCFShadowMap") {
        return luna::SceneShadowMode::PcfShadowMap;
    }
    if (mode == "CascadedShadowMaps" || mode == "CSM") {
        return luna::SceneShadowMode::CascadedShadowMaps;
    }

    return fallback;
}

bool sceneBackgroundModeHasVisibleSky(luna::SceneBackgroundMode mode)
{
    return mode != luna::SceneBackgroundMode::SolidColor;
}

uint32_t sanitizeShadowMapSize(uint32_t size, uint32_t fallback)
{
    constexpr uint32_t kMinShadowMapSize = 256;
    constexpr uint32_t kMaxShadowMapSize = 8192;
    return std::clamp(size == 0 ? fallback : size, kMinShadowMapSize, kMaxShadowMapSize);
}

const char* scriptPropertyTypeToString(luna::ScriptPropertyType type)
{
    switch (type) {
        case luna::ScriptPropertyType::Bool:
            return "Bool";
        case luna::ScriptPropertyType::Int:
            return "Int";
        case luna::ScriptPropertyType::Float:
            return "Float";
        case luna::ScriptPropertyType::String:
            return "String";
        case luna::ScriptPropertyType::Vec3:
            return "Vec3";
        case luna::ScriptPropertyType::Entity:
            return "Entity";
        case luna::ScriptPropertyType::Asset:
            return "Asset";
    }

    return "Float";
}

luna::ScriptPropertyType readScriptPropertyType(const YAML::Node& node, luna::ScriptPropertyType fallback)
{
    if (!node) {
        return fallback;
    }

    const std::string type = node.as<std::string>();
    if (type == "Bool") {
        return luna::ScriptPropertyType::Bool;
    }
    if (type == "Int") {
        return luna::ScriptPropertyType::Int;
    }
    if (type == "Float") {
        return luna::ScriptPropertyType::Float;
    }
    if (type == "String") {
        return luna::ScriptPropertyType::String;
    }
    if (type == "Vec3") {
        return luna::ScriptPropertyType::Vec3;
    }
    if (type == "Entity") {
        return luna::ScriptPropertyType::Entity;
    }
    if (type == "Asset") {
        return luna::ScriptPropertyType::Asset;
    }

    return fallback;
}

void emitScriptProperty(YAML::Emitter& out, const luna::ScriptProperty& property)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Name" << YAML::Value << property.name;
    out << YAML::Key << "Type" << YAML::Value << scriptPropertyTypeToString(property.type);
    out << YAML::Key << "Value" << YAML::Value;
    switch (property.type) {
        case luna::ScriptPropertyType::Bool:
            out << property.boolValue;
            break;
        case luna::ScriptPropertyType::Int:
            out << property.intValue;
            break;
        case luna::ScriptPropertyType::Float:
            out << property.floatValue;
            break;
        case luna::ScriptPropertyType::String:
            out << property.stringValue;
            break;
        case luna::ScriptPropertyType::Vec3:
            emitVec3(out, property.vec3Value);
            break;
        case luna::ScriptPropertyType::Entity:
            out << static_cast<uint64_t>(property.entityValue);
            break;
        case luna::ScriptPropertyType::Asset:
            out << static_cast<uint64_t>(property.assetValue);
            break;
    }
    out << YAML::EndMap;
}

luna::ScriptProperty readScriptProperty(const YAML::Node& node)
{
    luna::ScriptProperty property{};
    if (!node) {
        return property;
    }

    if (node["Name"]) {
        property.name = node["Name"].as<std::string>();
    }
    property.type = readScriptPropertyType(node["Type"], property.type);

    const YAML::Node value_node = node["Value"];
    if (!value_node) {
        return property;
    }

    switch (property.type) {
        case luna::ScriptPropertyType::Bool:
            property.boolValue = value_node.as<bool>();
            break;
        case luna::ScriptPropertyType::Int:
            property.intValue = value_node.as<int>();
            break;
        case luna::ScriptPropertyType::Float:
            property.floatValue = value_node.as<float>();
            break;
        case luna::ScriptPropertyType::String:
            property.stringValue = value_node.as<std::string>();
            break;
        case luna::ScriptPropertyType::Vec3:
            property.vec3Value = readVec3(value_node, property.vec3Value);
            break;
        case luna::ScriptPropertyType::Entity:
            property.entityValue = luna::UUID(value_node.as<uint64_t>());
            break;
        case luna::ScriptPropertyType::Asset:
            property.assetValue = luna::AssetHandle(value_node.as<uint64_t>());
            break;
    }

    return property;
}

void emitSceneEnvironment(YAML::Emitter& out, const luna::SceneEnvironmentSettings& environment)
{
    out << YAML::Key << "Environment" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Enabled" << YAML::Value << sceneBackgroundModeHasVisibleSky(environment.backgroundMode);
    out << YAML::Key << "BackgroundMode" << YAML::Value << sceneBackgroundModeToString(environment.backgroundMode);
    out << YAML::Key << "BackgroundColor" << YAML::Value;
    emitVec3(out, environment.backgroundColor);
    out << YAML::Key << "IblEnabled" << YAML::Value << environment.iblEnabled;
    out << YAML::Key << "EnvironmentMapHandle" << YAML::Value
        << static_cast<uint64_t>(environment.environmentMapHandle);
    out << YAML::Key << "Intensity" << YAML::Value << environment.intensity;
    out << YAML::Key << "SkyIntensity" << YAML::Value << environment.skyIntensity;
    out << YAML::Key << "DiffuseIntensity" << YAML::Value << environment.diffuseIntensity;
    out << YAML::Key << "SpecularIntensity" << YAML::Value << environment.specularIntensity;
    out << YAML::Key << "ProceduralSunDirection" << YAML::Value;
    emitVec3(out, environment.proceduralSunDirection);
    out << YAML::Key << "ProceduralSunIntensity" << YAML::Value << environment.proceduralSunIntensity;
    out << YAML::Key << "ProceduralSunAngularRadius" << YAML::Value << environment.proceduralSunAngularRadius;
    out << YAML::Key << "ProceduralSkyColorZenith" << YAML::Value;
    emitVec3(out, environment.proceduralSkyColorZenith);
    out << YAML::Key << "ProceduralSkyColorHorizon" << YAML::Value;
    emitVec3(out, environment.proceduralSkyColorHorizon);
    out << YAML::Key << "ProceduralGroundColor" << YAML::Value;
    emitVec3(out, environment.proceduralGroundColor);
    out << YAML::Key << "ProceduralSkyExposure" << YAML::Value << environment.proceduralSkyExposure;
    out << YAML::EndMap;
}

void emitSceneShadows(YAML::Emitter& out, const luna::SceneShadowSettings& shadows)
{
    out << YAML::Key << "Shadows" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Mode" << YAML::Value << sceneShadowModeToString(shadows.mode);
    out << YAML::Key << "PcfShadowDistance" << YAML::Value << shadows.pcfShadowDistance;
    out << YAML::Key << "PcfMapSize" << YAML::Value << shadows.pcfMapSize;
    out << YAML::Key << "CsmCascadeSize" << YAML::Value << shadows.csmCascadeSize;
    out << YAML::EndMap;
}

void readSceneEnvironment(const YAML::Node& node, luna::SceneEnvironmentSettings& environment)
{
    if (!node) {
        return;
    }

    bool legacy_enabled = environment.enabled;
    bool has_legacy_enabled = false;
    if (node["Enabled"]) {
        legacy_enabled = node["Enabled"].as<bool>();
        has_legacy_enabled = true;
    }
    if (node["IblEnabled"]) {
        environment.iblEnabled = node["IblEnabled"].as<bool>();
    }
    if (node["EnvironmentMapHandle"]) {
        environment.environmentMapHandle = luna::AssetHandle(node["EnvironmentMapHandle"].as<uint64_t>());
    }
    if (node["BackgroundMode"]) {
        environment.backgroundMode = readSceneBackgroundMode(node["BackgroundMode"], environment.backgroundMode);
    } else if (has_legacy_enabled && !legacy_enabled) {
        environment.backgroundMode = luna::SceneBackgroundMode::SolidColor;
    } else if (environment.environmentMapHandle.isValid()) {
        environment.backgroundMode = luna::SceneBackgroundMode::EnvironmentMap;
    } else {
        environment.backgroundMode = luna::SceneBackgroundMode::ProceduralSky;
    }
    environment.backgroundColor = readVec3(node["BackgroundColor"], environment.backgroundColor);
    environment.enabled = sceneBackgroundModeHasVisibleSky(environment.backgroundMode);
    if (node["Intensity"]) {
        environment.intensity = node["Intensity"].as<float>();
    }
    if (node["SkyIntensity"]) {
        environment.skyIntensity = node["SkyIntensity"].as<float>();
    }
    if (node["DiffuseIntensity"]) {
        environment.diffuseIntensity = node["DiffuseIntensity"].as<float>();
    }
    if (node["SpecularIntensity"]) {
        environment.specularIntensity = node["SpecularIntensity"].as<float>();
    }
    environment.proceduralSunDirection =
        readVec3(node["ProceduralSunDirection"], environment.proceduralSunDirection);
    if (node["ProceduralSunIntensity"]) {
        environment.proceduralSunIntensity = node["ProceduralSunIntensity"].as<float>();
    }
    if (node["ProceduralSunAngularRadius"]) {
        environment.proceduralSunAngularRadius = node["ProceduralSunAngularRadius"].as<float>();
    }
    environment.proceduralSkyColorZenith =
        readVec3(node["ProceduralSkyColorZenith"], environment.proceduralSkyColorZenith);
    environment.proceduralSkyColorHorizon =
        readVec3(node["ProceduralSkyColorHorizon"], environment.proceduralSkyColorHorizon);
    environment.proceduralGroundColor = readVec3(node["ProceduralGroundColor"], environment.proceduralGroundColor);
    if (node["ProceduralSkyExposure"]) {
        environment.proceduralSkyExposure = node["ProceduralSkyExposure"].as<float>();
    }
}

void readSceneShadows(const YAML::Node& node, luna::SceneShadowSettings& shadows)
{
    if (!node) {
        return;
    }

    if (node["Mode"]) {
        shadows.mode = readSceneShadowMode(node["Mode"], shadows.mode);
    } else if (node["CascadedShadowsEnabled"]) {
        shadows.mode = node["CascadedShadowsEnabled"].as<bool>() ? luna::SceneShadowMode::CascadedShadowMaps
                                                                 : luna::SceneShadowMode::None;
    }
    if (node["PcfShadowDistance"]) {
        shadows.pcfShadowDistance = std::clamp(node["PcfShadowDistance"].as<float>(), 1.0f, 1000.0f);
    }
    if (node["PcfMapSize"]) {
        shadows.pcfMapSize = sanitizeShadowMapSize(node["PcfMapSize"].as<uint32_t>(), shadows.pcfMapSize);
    }
    if (node["CsmCascadeSize"]) {
        shadows.csmCascadeSize = sanitizeShadowMapSize(node["CsmCascadeSize"].as<uint32_t>(), shadows.csmCascadeSize);
    }
}

const char* lightTypeToString(luna::LightComponent::Type type)
{
    switch (type) {
        case luna::LightComponent::Type::Directional:
            return "Directional";
        case luna::LightComponent::Type::Point:
            return "Point";
        case luna::LightComponent::Type::Spot:
            return "Spot";
    }

    return "Directional";
}

luna::LightComponent::Type readLightType(const YAML::Node& node, luna::LightComponent::Type fallback)
{
    if (!node) {
        return fallback;
    }

    const std::string type = node.as<std::string>();
    if (type == "Directional") {
        return luna::LightComponent::Type::Directional;
    }
    if (type == "Point") {
        return luna::LightComponent::Type::Point;
    }
    if (type == "Spot") {
        return luna::LightComponent::Type::Spot;
    }

    return fallback;
}

bool emitSceneYaml(const luna::Scene& scene, YAML::Emitter& out, std::string_view source_name)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Scene" << YAML::Value << scene.getName();
    emitSceneEnvironment(out, scene.environmentSettings());
    emitSceneShadows(out, scene.shadowSettings());
    out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

    const auto& registry = scene.entityManager().registry();
    auto view = registry.view<const luna::IDComponent>();
    for (const auto entity_handle : view) {
        const auto& id_component = registry.get<const luna::IDComponent>(entity_handle);

        out << YAML::BeginMap;

        out << YAML::Key << "IDComponent" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "ID" << YAML::Value << static_cast<uint64_t>(id_component.id);
        out << YAML::EndMap;

        if (registry.all_of<luna::TagComponent>(entity_handle)) {
            const auto& tag_component = registry.get<const luna::TagComponent>(entity_handle);
            out << YAML::Key << "TagComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << tag_component.tag;
            out << YAML::EndMap;
        }

        if (registry.all_of<luna::TransformComponent>(entity_handle)) {
            const auto& transform_component = registry.get<const luna::TransformComponent>(entity_handle);
            out << YAML::Key << "TransformComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Translation" << YAML::Value;
            emitVec3(out, transform_component.translation);
            out << YAML::Key << "Rotation" << YAML::Value;
            emitVec3(out, transform_component.rotation);
            out << YAML::Key << "Scale" << YAML::Value;
            emitVec3(out, transform_component.scale);
            out << YAML::EndMap;
        }

        if (registry.all_of<luna::RelationshipComponent>(entity_handle)) {
            const auto& relationship_component = registry.get<const luna::RelationshipComponent>(entity_handle);
            out << YAML::Key << "RelationshipComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ParentHandle" << YAML::Value
                << static_cast<uint64_t>(relationship_component.parentHandle);
            out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
            for (const luna::UUID child_uuid : relationship_component.children) {
                out << static_cast<uint64_t>(child_uuid);
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        if (registry.all_of<luna::CameraComponent>(entity_handle)) {
            const auto& camera_component = registry.get<const luna::CameraComponent>(entity_handle);
            out << YAML::Key << "CameraComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Primary" << YAML::Value << camera_component.primary;
            out << YAML::Key << "FixedAspectRatio" << YAML::Value << camera_component.fixedAspectRatio;
            out << YAML::Key << "ProjectionType" << YAML::Value
                << (camera_component.projectionType == luna::Camera::ProjectionType::Orthographic ? "Orthographic"
                                                                                                   : "Perspective");
            out << YAML::Key << "PerspectiveFOV" << YAML::Value
                << glm::degrees(camera_component.perspectiveVerticalFovRadians);
            out << YAML::Key << "PerspectiveNear" << YAML::Value << camera_component.perspectiveNear;
            out << YAML::Key << "PerspectiveFar" << YAML::Value << camera_component.perspectiveFar;
            out << YAML::Key << "OrthographicSize" << YAML::Value << camera_component.orthographicSize;
            out << YAML::Key << "OrthographicNear" << YAML::Value << camera_component.orthographicNear;
            out << YAML::Key << "OrthographicFar" << YAML::Value << camera_component.orthographicFar;
            out << YAML::EndMap;
        }

        if (registry.all_of<luna::LightComponent>(entity_handle)) {
            const auto& light_component = registry.get<const luna::LightComponent>(entity_handle);
            out << YAML::Key << "LightComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << lightTypeToString(light_component.type);
            out << YAML::Key << "Enabled" << YAML::Value << light_component.enabled;
            out << YAML::Key << "Color" << YAML::Value;
            emitVec3(out, light_component.color);
            out << YAML::Key << "Intensity" << YAML::Value << light_component.intensity;
            out << YAML::Key << "Range" << YAML::Value << light_component.range;
            out << YAML::Key << "InnerConeAngle" << YAML::Value << glm::degrees(light_component.innerConeAngleRadians);
            out << YAML::Key << "OuterConeAngle" << YAML::Value << glm::degrees(light_component.outerConeAngleRadians);
            out << YAML::EndMap;
        }

        if (registry.all_of<luna::MeshComponent>(entity_handle)) {
            const auto& mesh_component = registry.get<const luna::MeshComponent>(entity_handle);
            out << YAML::Key << "MeshComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "MeshHandle" << YAML::Value << static_cast<uint64_t>(mesh_component.meshHandle);
            out << YAML::Key << "SubmeshMaterials" << YAML::Value << YAML::BeginSeq;
            for (const luna::AssetHandle material_handle : mesh_component.submeshMaterials) {
                out << static_cast<uint64_t>(material_handle);
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        if (registry.all_of<luna::ScriptComponent>(entity_handle)) {
            const auto& script_component = registry.get<const luna::ScriptComponent>(entity_handle);
            out << YAML::Key << "ScriptComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Enabled" << YAML::Value << script_component.enabled;
            out << YAML::Key << "Scripts" << YAML::Value << YAML::BeginSeq;
            for (const auto& script : script_component.scripts) {
                out << YAML::BeginMap;
                out << YAML::Key << "ID" << YAML::Value << static_cast<uint64_t>(script.id);
                out << YAML::Key << "Enabled" << YAML::Value << script.enabled;
                out << YAML::Key << "ScriptAsset" << YAML::Value << static_cast<uint64_t>(script.scriptAsset);
                out << YAML::Key << "TypeName" << YAML::Value << script.typeName;
                out << YAML::Key << "ExecutionOrder" << YAML::Value << script.executionOrder;
                out << YAML::Key << "Properties" << YAML::Value << YAML::BeginSeq;
                for (const auto& property : script.properties) {
                    emitScriptProperty(out, property);
                }
                out << YAML::EndSeq;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        out << YAML::EndMap;
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;

    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to emit scene YAML for '{}': {}", source_name, out.GetLastError());
        return false;
    }

    return true;
}

} // namespace

namespace luna {

namespace {

bool deserializeSceneFromNode(Scene& scene, const YAML::Node& data, std::string scene_name, std::string_view source_name)
{
    if (data["Scene"]) {
        scene_name = data["Scene"].as<std::string>();
    }

    std::vector<SerializedEntityData> entities;
    SceneEnvironmentSettings environment_settings{};
    SceneShadowSettings shadow_settings{};
    try {
        if (const YAML::Node environment_node = data["Environment"]; environment_node) {
            readSceneEnvironment(environment_node, environment_settings);
        }
        if (const YAML::Node shadows_node = data["Shadows"]; shadows_node) {
            readSceneShadows(shadows_node, shadow_settings);
        }

        const YAML::Node entities_node = data["Entities"];
        if (entities_node && !entities_node.IsSequence()) {
            LUNA_CORE_ERROR("Scene data '{}' has an invalid 'Entities' node", source_name);
            return false;
        }

        if (entities_node) {
            entities.reserve(entities_node.size());
            for (const auto& entity_node : entities_node) {
                SerializedEntityData entity_data;

                if (const YAML::Node id_component = entity_node["IDComponent"]; id_component && id_component["ID"]) {
                    entity_data.id = id_component["ID"].as<uint64_t>();
                }

                if (const YAML::Node tag_component = entity_node["TagComponent"]; tag_component && tag_component["Tag"]) {
                    entity_data.tag = tag_component["Tag"].as<std::string>();
                }

                if (const YAML::Node transform_component = entity_node["TransformComponent"]; transform_component) {
                    entity_data.transform.translation =
                        readVec3(transform_component["Translation"], entity_data.transform.translation);
                    entity_data.transform.rotation =
                        readVec3(transform_component["Rotation"], entity_data.transform.rotation);
                    entity_data.transform.scale = readVec3(transform_component["Scale"], entity_data.transform.scale);
                }

                if (const YAML::Node relationship_component = entity_node["RelationshipComponent"]; relationship_component) {
                    if (relationship_component["ParentHandle"]) {
                        entity_data.parent_id = relationship_component["ParentHandle"].as<uint64_t>();
                    }

                    if (const YAML::Node children_node = relationship_component["Children"];
                        children_node && children_node.IsSequence()) {
                        entity_data.serialized_children.reserve(children_node.size());
                        for (const auto& child_node : children_node) {
                            entity_data.serialized_children.emplace_back(child_node.as<uint64_t>());
                        }
                    }
                }

                if (const YAML::Node camera_component = entity_node["CameraComponent"]; camera_component) {
                    entity_data.has_camera_component = true;
                    if (camera_component["Primary"]) {
                        entity_data.camera.primary = camera_component["Primary"].as<bool>();
                    }
                    if (camera_component["FixedAspectRatio"]) {
                        entity_data.camera.fixedAspectRatio = camera_component["FixedAspectRatio"].as<bool>();
                    }
                    if (camera_component["ProjectionType"]) {
                        const std::string projection_type = camera_component["ProjectionType"].as<std::string>();
                        entity_data.camera.projectionType = projection_type == "Orthographic"
                                                              ? Camera::ProjectionType::Orthographic
                                                              : Camera::ProjectionType::Perspective;
                    }
                    if (camera_component["PerspectiveFOV"]) {
                        entity_data.camera.perspectiveVerticalFovRadians =
                            glm::radians(camera_component["PerspectiveFOV"].as<float>());
                    }
                    if (camera_component["PerspectiveNear"]) {
                        entity_data.camera.perspectiveNear = camera_component["PerspectiveNear"].as<float>();
                    }
                    if (camera_component["PerspectiveFar"]) {
                        entity_data.camera.perspectiveFar = camera_component["PerspectiveFar"].as<float>();
                    }
                    if (camera_component["OrthographicSize"]) {
                        entity_data.camera.orthographicSize = camera_component["OrthographicSize"].as<float>();
                    }
                    if (camera_component["OrthographicNear"]) {
                        entity_data.camera.orthographicNear = camera_component["OrthographicNear"].as<float>();
                    }
                    if (camera_component["OrthographicFar"]) {
                        entity_data.camera.orthographicFar = camera_component["OrthographicFar"].as<float>();
                    }
                }

                if (const YAML::Node light_component = entity_node["LightComponent"]; light_component) {
                    entity_data.has_light_component = true;
                    entity_data.light.type = readLightType(light_component["Type"], entity_data.light.type);
                    if (light_component["Enabled"]) {
                        entity_data.light.enabled = light_component["Enabled"].as<bool>();
                    }
                    entity_data.light.color = readVec3(light_component["Color"], entity_data.light.color);
                    if (light_component["Intensity"]) {
                        entity_data.light.intensity = light_component["Intensity"].as<float>();
                    }
                    if (light_component["Range"]) {
                        entity_data.light.range = light_component["Range"].as<float>();
                    }
                    if (light_component["InnerConeAngle"]) {
                        entity_data.light.innerConeAngleRadians =
                            glm::radians(light_component["InnerConeAngle"].as<float>());
                    }
                    if (light_component["OuterConeAngle"]) {
                        entity_data.light.outerConeAngleRadians =
                            glm::radians(light_component["OuterConeAngle"].as<float>());
                    }
                }

                if (const YAML::Node mesh_component = entity_node["MeshComponent"]; mesh_component) {
                    entity_data.has_mesh_component = true;

                    if (mesh_component["MeshHandle"]) {
                        entity_data.mesh.meshHandle = AssetHandle(mesh_component["MeshHandle"].as<uint64_t>());
                    }

                    if (const YAML::Node materials_node = mesh_component["SubmeshMaterials"];
                        materials_node && materials_node.IsSequence()) {
                        entity_data.mesh.submeshMaterials.reserve(materials_node.size());
                        for (const auto& material_node : materials_node) {
                            entity_data.mesh.submeshMaterials.emplace_back(material_node.as<uint64_t>());
                        }
                    }
                }

                if (const YAML::Node script_component = entity_node["ScriptComponent"]; script_component) {
                    entity_data.has_script_component = true;
                    if (script_component["Enabled"]) {
                        entity_data.script.enabled = script_component["Enabled"].as<bool>();
                    }

                    if (const YAML::Node scripts_node = script_component["Scripts"];
                        scripts_node && scripts_node.IsSequence()) {
                        entity_data.script.scripts.reserve(scripts_node.size());
                        for (const auto& script_node : scripts_node) {
                            ScriptEntry script{};
                            if (script_node["ID"]) {
                                script.id = UUID(script_node["ID"].as<uint64_t>());
                            }
                            if (script_node["Enabled"]) {
                                script.enabled = script_node["Enabled"].as<bool>();
                            }
                            if (script_node["ScriptAsset"]) {
                                script.scriptAsset = AssetHandle(script_node["ScriptAsset"].as<uint64_t>());
                            }
                            if (script_node["TypeName"]) {
                                script.typeName = script_node["TypeName"].as<std::string>();
                            }
                            if (script_node["ExecutionOrder"]) {
                                script.executionOrder = script_node["ExecutionOrder"].as<int>();
                            }

                            if (const YAML::Node properties_node = script_node["Properties"];
                                properties_node && properties_node.IsSequence()) {
                                script.properties.reserve(properties_node.size());
                                for (const auto& property_node : properties_node) {
                                    script.properties.push_back(readScriptProperty(property_node));
                                }
                            }

                            entity_data.script.scripts.push_back(std::move(script));
                        }
                    }
                }

                entities.push_back(std::move(entity_data));
            }
        }
    } catch (const YAML::Exception& error) {
        LUNA_CORE_ERROR("Failed to deserialize scene data from '{}': {}", source_name, error.what());
        return false;
    }

    scene.entityManager().clear();
    scene.setName(scene_name);
    scene.environmentSettings() = environment_settings;
    scene.shadowSettings() = shadow_settings;

    auto& entity_manager = scene.entityManager();
    for (const auto& entity_data : entities) {
        const UUID uuid = entity_data.id != 0 ? UUID(entity_data.id) : UUID{};
        Entity entity = entity_manager.createEntityWithUUID(uuid, entity_data.tag);

        auto& transform_component = entity.getComponent<TransformComponent>();
        transform_component = entity_data.transform;

        if (entity_data.has_camera_component) {
            auto& camera_component = entity.addComponent<CameraComponent>();
            camera_component = entity_data.camera;
        }

        if (entity_data.has_light_component) {
            auto& light_component = entity.addComponent<LightComponent>();
            light_component = entity_data.light;
        }

        if (entity_data.has_mesh_component) {
            auto& mesh_component = entity.addComponent<MeshComponent>();
            mesh_component = entity_data.mesh;
        }

        if (entity_data.has_script_component) {
            auto& script_component = entity.addComponent<ScriptComponent>();
            script_component = entity_data.script;
        }
    }

    for (const auto& entity_data : entities) {
        const UUID entity_uuid = entity_data.id != 0 ? UUID(entity_data.id) : UUID{};
        Entity entity = entity_manager.findEntityByUUID(entity_uuid);
        if (!entity) {
            continue;
        }

        const UUID parent_uuid = entity_data.parent_id != 0 ? UUID(entity_data.parent_id) : UUID(0);
        if (!parent_uuid.isValid()) {
            continue;
        }

        Entity parent = entity_manager.findEntityByUUID(parent_uuid);
        if (!parent) {
            LUNA_CORE_WARN("Scene '{}' references missing parent '{}' for entity '{}'",
                           scene_name,
                           parent_uuid.toString(),
                           entity.getUUID().toString());
            continue;
        }

        if (!entity_manager.setParent(entity, parent, false)) {
            LUNA_CORE_WARN("Failed to rebuild parent relationship '{}' -> '{}'",
                           entity.getUUID().toString(),
                           parent_uuid.toString());
        }
    }

    return true;
}

} // namespace

std::filesystem::path SceneSerializer::normalizeScenePath(const std::filesystem::path& scene_path)
{
    if (scene_path.empty()) {
        return {};
    }

    std::filesystem::path normalized_path = scene_path.lexically_normal();
    if (normalized_path.extension() != FileExtension) {
        normalized_path.replace_extension(FileExtension);
    }

    return normalized_path;
}

std::string SceneSerializer::serializeToString(const Scene& scene)
{
    YAML::Emitter out;
    if (!emitSceneYaml(scene, out, "memory scene snapshot")) {
        return {};
    }

    return out.c_str();
}

bool SceneSerializer::serialize(const Scene& scene, const std::filesystem::path& scene_path)
{
    const std::filesystem::path normalized_path = normalizeScenePath(scene_path);
    if (normalized_path.empty()) {
        LUNA_CORE_ERROR("Cannot serialize scene because the target path is empty");
        return false;
    }

    std::error_code ec;
    if (!normalized_path.parent_path().empty()) {
        std::filesystem::create_directories(normalized_path.parent_path(), ec);
        if (ec) {
            LUNA_CORE_ERROR("Failed to create scene directory '{}': {}",
                            normalized_path.parent_path().string(),
                            ec.message());
            return false;
        }
    }

    YAML::Emitter out;
    if (!emitSceneYaml(scene, out, normalized_path.string())) {
        return false;
    }

    std::ofstream output_stream(normalized_path);
    if (!output_stream.is_open()) {
        LUNA_CORE_ERROR("Failed to open scene file for writing: {}", normalized_path.string());
        return false;
    }

    output_stream << out.c_str();
    if (!output_stream.good()) {
        LUNA_CORE_ERROR("Failed to write scene file: {}", normalized_path.string());
        return false;
    }

    LUNA_CORE_INFO("Serialized scene '{}' with {} entities to '{}'",
                   scene.getName(),
                   scene.entityManager().entityCount(),
                   normalized_path.string());
    return true;
}

bool SceneSerializer::deserializeFromString(Scene& scene, std::string_view scene_data, std::string_view source_name)
{
    if (scene_data.empty()) {
        LUNA_CORE_ERROR("Cannot deserialize scene because the source data is empty");
        return false;
    }

    const std::string source_label = source_name.empty() ? "memory scene snapshot" : std::string(source_name);
    YAML::Node data;
    try {
        data = YAML::Load(std::string(scene_data));
    } catch (const YAML::Exception& error) {
        LUNA_CORE_ERROR("Failed to parse scene data '{}': {}", source_label, error.what());
        return false;
    }

    if (!deserializeSceneFromNode(scene, data, scene.getName(), source_label)) {
        return false;
    }

    LUNA_CORE_INFO("Deserialized scene '{}' with {} entities from '{}'",
                   scene.getName(),
                   scene.entityManager().entityCount(),
                   source_label);
    return true;
}

bool SceneSerializer::deserialize(Scene& scene, const std::filesystem::path& scene_path)
{
    const std::filesystem::path normalized_path = normalizeScenePath(scene_path);
    if (normalized_path.empty()) {
        LUNA_CORE_ERROR("Cannot deserialize scene because the source path is empty");
        return false;
    }

    if (!std::filesystem::exists(normalized_path)) {
        LUNA_CORE_ERROR("Scene file does not exist: {}", normalized_path.string());
        return false;
    }

    YAML::Node data;
    try {
        data = YAML::LoadFile(normalized_path.string());
    } catch (const YAML::Exception& error) {
        LUNA_CORE_ERROR("Failed to parse scene file '{}': {}", normalized_path.string(), error.what());
        return false;
    }

    if (!deserializeSceneFromNode(scene, data, normalized_path.stem().string(), normalized_path.string())) {
        return false;
    }

    LUNA_CORE_INFO("Deserialized scene '{}' with {} entities from '{}'",
                   scene.getName(),
                   scene.entityManager().entityCount(),
                   normalized_path.string());
    return true;
}

} // namespace luna
