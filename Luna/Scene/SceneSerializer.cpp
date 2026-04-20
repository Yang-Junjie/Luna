#include "SceneSerializer.h"

#include "Core/Log.h"
#include "Entity.h"
#include "Scene.h"

#include <fstream>
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
