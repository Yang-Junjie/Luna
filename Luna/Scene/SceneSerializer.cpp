#include "SceneSerializer.h"

#include "Core/Log.h"
#include "Entity.h"
#include "Scene.h"

#include <fstream>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <string>
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

} // namespace

namespace luna {

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
    out << YAML::BeginMap;
    out << YAML::Key << "Scene" << YAML::Value << scene.getName();
    out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

    const auto& registry = scene.entityManager().registry();
    auto view = registry.view<const IDComponent>();
    for (const auto entity_handle : view) {
        const auto& id_component = registry.get<const IDComponent>(entity_handle);

        out << YAML::BeginMap;

        out << YAML::Key << "IDComponent" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "ID" << YAML::Value << static_cast<uint64_t>(id_component.id);
        out << YAML::EndMap;

        if (registry.all_of<TagComponent>(entity_handle)) {
            const auto& tag_component = registry.get<const TagComponent>(entity_handle);
            out << YAML::Key << "TagComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << tag_component.tag;
            out << YAML::EndMap;
        }

        if (registry.all_of<TransformComponent>(entity_handle)) {
            const auto& transform_component = registry.get<const TransformComponent>(entity_handle);
            out << YAML::Key << "TransformComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Translation" << YAML::Value;
            emitVec3(out, transform_component.translation);
            out << YAML::Key << "Rotation" << YAML::Value;
            emitVec3(out, transform_component.rotation);
            out << YAML::Key << "Scale" << YAML::Value;
            emitVec3(out, transform_component.scale);
            out << YAML::EndMap;
        }

        if (registry.all_of<RelationshipComponent>(entity_handle)) {
            const auto& relationship_component = registry.get<const RelationshipComponent>(entity_handle);
            out << YAML::Key << "RelationshipComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ParentHandle" << YAML::Value
                << static_cast<uint64_t>(relationship_component.parentHandle);
            out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
            for (const UUID child_uuid : relationship_component.children) {
                out << static_cast<uint64_t>(child_uuid);
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        if (registry.all_of<CameraComponent>(entity_handle)) {
            const auto& camera_component = registry.get<const CameraComponent>(entity_handle);
            out << YAML::Key << "CameraComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Primary" << YAML::Value << camera_component.primary;
            out << YAML::Key << "FixedAspectRatio" << YAML::Value << camera_component.fixedAspectRatio;
            out << YAML::Key << "ProjectionType" << YAML::Value
                << (camera_component.projectionType == Camera::ProjectionType::Orthographic ? "Orthographic"
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

        if (registry.all_of<LightComponent>(entity_handle)) {
            const auto& light_component = registry.get<const LightComponent>(entity_handle);
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

        if (registry.all_of<MeshComponent>(entity_handle)) {
            const auto& mesh_component = registry.get<const MeshComponent>(entity_handle);
            out << YAML::Key << "MeshComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "MeshHandle" << YAML::Value << static_cast<uint64_t>(mesh_component.meshHandle);
            out << YAML::Key << "SubmeshMaterials" << YAML::Value << YAML::BeginSeq;
            for (const AssetHandle material_handle : mesh_component.submeshMaterials) {
                out << static_cast<uint64_t>(material_handle);
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        out << YAML::EndMap;
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;

    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to emit scene YAML for '{}': {}", normalized_path.string(), out.GetLastError());
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

    std::string scene_name = normalized_path.stem().string();
    if (data["Scene"]) {
        scene_name = data["Scene"].as<std::string>();
    }

    std::vector<SerializedEntityData> entities;
    try {
        const YAML::Node entities_node = data["Entities"];
        if (entities_node && !entities_node.IsSequence()) {
            LUNA_CORE_ERROR("Scene file '{}' has an invalid 'Entities' node", normalized_path.string());
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

                entities.push_back(std::move(entity_data));
            }
        }
    } catch (const YAML::Exception& error) {
        LUNA_CORE_ERROR("Failed to deserialize scene data from '{}': {}", normalized_path.string(), error.what());
        return false;
    }

    scene.entityManager().clear();
    scene.setName(scene_name);

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

    LUNA_CORE_INFO("Deserialized scene '{}' with {} entities from '{}'",
                   scene.getName(),
                   scene.entityManager().entityCount(),
                   normalized_path.string());
    return true;
}

} // namespace luna
