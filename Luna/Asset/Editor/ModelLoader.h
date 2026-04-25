#pragma once

#include "Asset/AssetManager.h"
#include "Asset/Model.h"
#include "Loader.h"
#include "Project/ProjectManager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <yaml-cpp/yaml.h>

namespace luna::model_loader_detail {

inline glm::vec3 readVec3(const YAML::Node& node, const glm::vec3& default_value)
{
    if (!node || !node.IsSequence() || node.size() < 3) {
        return default_value;
    }

    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>()};
}

inline AssetHandle readHandle(const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        return AssetHandle(0);
    }

    return AssetHandle(node.as<uint64_t>());
}

inline std::vector<AssetHandle> readHandleSequence(const YAML::Node& node)
{
    std::vector<AssetHandle> handles;
    if (!node || !node.IsSequence()) {
        return handles;
    }

    handles.reserve(node.size());
    for (const auto& item : node) {
        AssetHandle handle(0);
        if (item.IsScalar()) {
            handle = AssetHandle(item.as<uint64_t>());
        } else if (item.IsMap()) {
            handle = readHandle(item["Handle"]);
        }
        handles.push_back(handle);
    }

    return handles;
}

inline std::vector<AssetHandle> readMaterialBindings(const YAML::Node& node)
{
    std::vector<AssetHandle> materials;
    if (!node || !node.IsSequence()) {
        return materials;
    }

    for (const auto& item : node) {
        if (item.IsScalar()) {
            materials.push_back(AssetHandle(item.as<uint64_t>()));
            continue;
        }

        if (!item.IsMap()) {
            continue;
        }

        const uint32_t material_index =
            item["SourceMaterialIndex"] ? item["SourceMaterialIndex"].as<uint32_t>() : static_cast<uint32_t>(materials.size());
        if (materials.size() <= material_index) {
            materials.resize(static_cast<size_t>(material_index) + 1, AssetHandle(0));
        }
        materials[material_index] = readHandle(item["Handle"]);
    }

    return materials;
}

inline ModelNode readNode(const YAML::Node& node, const std::string& fallback_name)
{
    ModelNode model_node;
    model_node.Name = node["Name"] ? node["Name"].as<std::string>() : fallback_name;
    model_node.Parent = node["Parent"] ? node["Parent"].as<int32_t>() : -1;

    if (const YAML::Node children = node["Children"]; children && children.IsSequence()) {
        model_node.Children.reserve(children.size());
        for (const auto& child : children) {
            model_node.Children.push_back(child.as<uint32_t>());
        }
    }

    model_node.Translation = readVec3(node["Translation"], model_node.Translation);
    model_node.Rotation = readVec3(node["Rotation"], model_node.Rotation);
    model_node.Scale = readVec3(node["Scale"], model_node.Scale);

    if (node["Mesh"]) {
        model_node.MeshHandle = readHandle(node["Mesh"]);
    } else if (node["MeshHandle"]) {
        model_node.MeshHandle = readHandle(node["MeshHandle"]);
    }

    model_node.SubmeshMaterials = readHandleSequence(node["SubmeshMaterials"]);
    return model_node;
}

inline void normalizeHierarchy(std::vector<ModelNode>& nodes)
{
    for (size_t parent_index = 0; parent_index < nodes.size(); ++parent_index) {
        for (const uint32_t child_index : nodes[parent_index].Children) {
            if (child_index < nodes.size() && nodes[child_index].Parent < 0) {
                nodes[child_index].Parent = static_cast<int32_t>(parent_index);
            }
        }
    }

    for (auto& node : nodes) {
        node.Children.clear();
        if (node.Parent < 0 || static_cast<size_t>(node.Parent) >= nodes.size()) {
            node.Parent = -1;
        }
    }

    for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
        const int32_t parent_index = nodes[node_index].Parent;
        if (parent_index >= 0 && static_cast<size_t>(parent_index) < nodes.size()) {
            nodes[parent_index].Children.push_back(static_cast<uint32_t>(node_index));
        }
    }
}

inline std::shared_ptr<Model> loadFromYaml(const std::filesystem::path& path, std::string asset_name)
{
    YAML::Node data = YAML::LoadFile(path.string());
    YAML::Node model_node = data["Model"] ? data["Model"] : data;
    if (!model_node) {
        return {};
    }

    if (asset_name.empty()) {
        asset_name = model_node["Name"] ? model_node["Name"].as<std::string>() : path.stem().string();
    }

    const std::filesystem::path source_path =
        model_node["Source"] ? std::filesystem::path(model_node["Source"].as<std::string>()) : std::filesystem::path{};

    std::vector<ModelNode> nodes;
    if (const YAML::Node nodes_node = model_node["Nodes"]; nodes_node && nodes_node.IsSequence()) {
        nodes.reserve(nodes_node.size());
        for (size_t node_index = 0; node_index < nodes_node.size(); ++node_index) {
            nodes.push_back(readNode(nodes_node[node_index], asset_name + "_Node_" + std::to_string(node_index)));
        }
    }

    if (nodes.empty()) {
        AssetHandle source_mesh(0);
        if (model_node["SourceMesh"]) {
            source_mesh = readHandle(model_node["SourceMesh"]);
        } else if (model_node["Mesh"]) {
            source_mesh = readHandle(model_node["Mesh"]);
        }

        if (source_mesh.isValid()) {
            ModelNode fallback_node;
            fallback_node.Name = asset_name;
            fallback_node.MeshHandle = source_mesh;
            fallback_node.SubmeshMaterials = readMaterialBindings(model_node["Materials"]);
            if (fallback_node.SubmeshMaterials.empty()) {
                fallback_node.SubmeshMaterials = readHandleSequence(model_node["SubmeshMaterials"]);
            }
            nodes.push_back(std::move(fallback_node));
        }
    }

    normalizeHierarchy(nodes);
    return Model::create(std::move(asset_name), source_path, std::move(nodes));
}

} // namespace luna::model_loader_detail

namespace luna {

class ModelLoader final : public Loader {
public:
    std::shared_ptr<Asset> load(const AssetMetadata& meta_data) override
    {
        const auto project_root_path = ProjectManager::instance().getProjectRootPath();
        if (!project_root_path) {
            return {};
        }

        return loadFromFile(*project_root_path / meta_data.FilePath, meta_data.Name);
    }

    static std::shared_ptr<Model> loadFromFile(const std::filesystem::path& path, std::string asset_name = {})
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (extension == ".lmodel") {
            return model_loader_detail::loadFromYaml(path, std::move(asset_name));
        }

        return {};
    }
};

} // namespace luna
