#include "Asset/Editor/FbxModelAssetGenerator.h"

#include "Asset/AssetDatabase.h"
#include "Asset/Editor/Importer.h"
#include "Asset/Editor/MaterialFactory.h"
#include "Asset/Editor/MaterialImporter.h"
#include "Asset/Editor/ModelImporter.h"
#include "Asset/Editor/TextureImporter.h"
#include "Core/FileTool.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <ufbx.h>
#include <yaml-cpp/yaml.h>

namespace luna::fbx_model_asset_generator_detail {

enum class TextureRole {
    BaseColor,
    Normal,
    MetallicRoughness,
    Emissive,
    Occlusion,
};

struct GeneratedMaterial {
    uint32_t SourceMaterialIndex = 0;
    AssetHandle Handle{0};
    std::filesystem::path FilePath;
};

struct GeneratedMeshInfo {
    uint32_t FirstSubmesh = 0;
    uint32_t SubmeshCount = 0;
    std::vector<AssetHandle> SubmeshMaterials;
};

struct GeneratedNode {
    std::string Name;
    int32_t Parent = -1;
    glm::vec3 Translation{0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f, 1.0f, 1.0f};
    AssetHandle MeshHandle{0};
    uint32_t FirstSubmesh = 0;
    uint32_t SubmeshCount = UINT32_MAX;
    std::vector<AssetHandle> SubmeshMaterials;
};

using FbxScenePtr = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;
using MeshInfoMap = std::unordered_map<const ufbx_mesh*, GeneratedMeshInfo>;

std::string toString(ufbx_string value)
{
    if (value.data == nullptr || value.length == 0) {
        return {};
    }

    return std::string(value.data, value.length);
}

std::string sanitizeFileStem(std::string value)
{
    if (value.empty()) {
        return "Asset";
    }

    for (char& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isalnum(byte) && character != '_' && character != '-') {
            character = '_';
        }
    }

    while (!value.empty() && value.front() == '_') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '_') {
        value.pop_back();
    }

    return value.empty() ? "Asset" : value;
}

bool hasSupportedTextureExtension(const std::filesystem::path& texture_path)
{
    const std::string extension = importer_detail::normalizeExtension(texture_path);
    const std::array<std::string_view, 9> supported_extensions{
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".dds", ".ktx", ".ktx2"};

    return std::find(supported_extensions.begin(), supported_extensions.end(), extension) !=
           supported_extensions.end();
}

std::optional<std::filesystem::path> makeRelativeToProject(const std::filesystem::path& path)
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    if (!project_root || path.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(path.lexically_normal(), *project_root, ec);
    if (ec || relative_path.empty() || relative_path.is_absolute()) {
        return std::nullopt;
    }

    const std::string relative_string = relative_path.generic_string();
    if (relative_string == "." || relative_string.starts_with("..")) {
        return std::nullopt;
    }

    return relative_path.lexically_normal();
}

void emitVec3(YAML::Emitter& out, const glm::vec3& value)
{
    out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
}

void emitHandle(YAML::Emitter& out, AssetHandle handle)
{
    out << static_cast<uint64_t>(handle);
}

AssetHandle readHandle(const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        return AssetHandle(0);
    }

    return AssetHandle(node.as<uint64_t>());
}

glm::vec3 toVec3(const ufbx_vec3& value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

glm::quat toQuat(const ufbx_quat& value)
{
    return glm::quat(static_cast<float>(value.w),
                     static_cast<float>(value.x),
                     static_cast<float>(value.y),
                     static_cast<float>(value.z));
}

glm::vec3 toEulerRadians(const ufbx_quat& value)
{
    return glm::eulerAngles(glm::normalize(toQuat(value)));
}

GeneratedNode makeGeneratedNode(std::string name, int32_t parent, const ufbx_transform& transform)
{
    GeneratedNode node;
    node.Name = std::move(name);
    node.Parent = parent;
    node.Translation = toVec3(transform.translation);
    node.Rotation = toEulerRadians(transform.rotation);
    node.Scale = toVec3(transform.scale);
    return node;
}

bool isIdentityTransform(const ufbx_transform& transform)
{
    constexpr float kEpsilon = 0.00001f;
    const auto near_zero = [](double value) {
        return std::abs(value) < kEpsilon;
    };
    const auto near_one = [](double value) {
        return std::abs(value - 1.0) < kEpsilon;
    };

    return near_zero(transform.translation.x) && near_zero(transform.translation.y) &&
           near_zero(transform.translation.z) && near_zero(transform.rotation.x) &&
           near_zero(transform.rotation.y) && near_zero(transform.rotation.z) &&
           std::abs(std::abs(transform.rotation.w) - 1.0) < kEpsilon && near_one(transform.scale.x) &&
           near_one(transform.scale.y) && near_one(transform.scale.z);
}

void attachMeshInfo(GeneratedNode& node, const GeneratedMeshInfo& mesh_info, AssetHandle mesh_handle)
{
    if (mesh_info.SubmeshCount == 0) {
        return;
    }

    node.MeshHandle = mesh_handle;
    node.FirstSubmesh = mesh_info.FirstSubmesh;
    node.SubmeshCount = mesh_info.SubmeshCount;
    node.SubmeshMaterials = mesh_info.SubmeshMaterials;
}

FbxScenePtr loadFbxForCompanionGeneration(const std::filesystem::path& fbx_path)
{
    ufbx_load_opts opts{};
    opts.load_external_files = true;
    opts.ignore_missing_external_files = true;
    opts.generate_missing_normals = true;
    opts.use_blender_pbr_material = true;
    opts.target_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
    opts.target_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Y;
    opts.target_axes.front = UFBX_COORDINATE_AXIS_POSITIVE_Z;
    opts.target_unit_meters = 1.0f;

    const std::string path_string = fbx_path.string();
    ufbx_error error{};
    return FbxScenePtr(ufbx_load_file_len(path_string.data(), path_string.size(), &opts, &error), ufbx_free_scene);
}

uint32_t findSceneMaterialIndex(const ufbx_scene* scene, const ufbx_material* material)
{
    if (scene == nullptr || material == nullptr) {
        return UINT32_MAX;
    }

    for (size_t material_index = 0; material_index < scene->materials.count; ++material_index) {
        if (scene->materials.data[material_index] == material) {
            return static_cast<uint32_t>(material_index);
        }
    }

    return UINT32_MAX;
}

float mapReal(const ufbx_material_map& map, float default_value)
{
    return map.has_value ? static_cast<float>(map.value_real) : default_value;
}

glm::vec3 mapVec3(const ufbx_material_map& map, const glm::vec3& default_value)
{
    if (!map.has_value) {
        return default_value;
    }

    return {
        static_cast<float>(map.value_vec3.x),
        static_cast<float>(map.value_vec3.y),
        static_cast<float>(map.value_vec3.z),
    };
}

const ufbx_texture* resolveFileTexture(const ufbx_texture* texture)
{
    if (texture == nullptr) {
        return nullptr;
    }

    if (texture->type == UFBX_TEXTURE_FILE) {
        return texture;
    }

    for (size_t texture_index = 0; texture_index < texture->file_textures.count; ++texture_index) {
        if (const ufbx_texture* file_texture = resolveFileTexture(texture->file_textures.data[texture_index])) {
            return file_texture;
        }
    }

    for (size_t layer_index = 0; layer_index < texture->layers.count; ++layer_index) {
        if (const ufbx_texture* file_texture = resolveFileTexture(texture->layers.data[layer_index].texture)) {
            return file_texture;
        }
    }

    return texture;
}

std::optional<std::filesystem::path> normalizeExistingTexturePath(const std::filesystem::path& fbx_path,
                                                                  const std::string& texture_filename)
{
    if (texture_filename.empty()) {
        return std::nullopt;
    }

    std::filesystem::path candidate = texture_filename;
    if (!candidate.is_absolute()) {
        candidate = fbx_path.parent_path() / candidate;
    }
    candidate = candidate.lexically_normal();

    if (std::filesystem::exists(candidate) && hasSupportedTextureExtension(candidate)) {
        return candidate;
    }

    const std::filesystem::path sibling_candidate =
        (fbx_path.parent_path() / std::filesystem::path(texture_filename).filename()).lexically_normal();
    if (std::filesystem::exists(sibling_candidate) && hasSupportedTextureExtension(sibling_candidate)) {
        return sibling_candidate;
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> texturePathForTexture(const std::filesystem::path& fbx_path,
                                                           const ufbx_texture* texture)
{
    const ufbx_texture* file_texture = resolveFileTexture(texture);
    if (file_texture == nullptr) {
        return std::nullopt;
    }

    const std::array<std::string, 3> filenames{
        toString(file_texture->filename),
        toString(file_texture->relative_filename),
        toString(file_texture->absolute_filename),
    };

    for (const std::string& filename : filenames) {
        if (auto texture_path = normalizeExistingTexturePath(fbx_path, filename)) {
            return texture_path;
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> texturePathForMap(const std::filesystem::path& fbx_path,
                                                       const ufbx_material_map& map)
{
    if (map.texture == nullptr || !map.texture_enabled) {
        return std::nullopt;
    }

    return texturePathForTexture(fbx_path, map.texture);
}

std::string textureAssetName(const std::string& material_name, TextureRole role)
{
    switch (role) {
        case TextureRole::BaseColor:
            return material_name + "_BaseColor";
        case TextureRole::Normal:
            return material_name + "_Normal";
        case TextureRole::MetallicRoughness:
            return material_name + "_MetallicRoughness";
        case TextureRole::Emissive:
            return material_name + "_Emissive";
        case TextureRole::Occlusion:
            return material_name + "_Occlusion";
    }

    return material_name + "_Texture";
}

bool shouldUseSrgb(TextureRole role)
{
    return role == TextureRole::BaseColor || role == TextureRole::Emissive;
}

AssetMetadata ensureTextureMetadata(const std::filesystem::path& texture_path,
                                    const std::string& asset_name,
                                    TextureRole role,
                                    FbxModelAssetGenerator::GenerateResult& result)
{
    AssetMetadata metadata;
    if (texture_path.empty() || !std::filesystem::exists(texture_path) ||
        !hasSupportedTextureExtension(texture_path) || !makeRelativeToProject(texture_path).has_value()) {
        return metadata;
    }

    TextureImporter importer;
    const std::filesystem::path meta_path = importer_detail::getMetadataPath(texture_path);
    bool created_metadata = false;
    if (std::filesystem::exists(meta_path)) {
        metadata = importer.deserializeMetadata(meta_path);
    } else {
        metadata = importer.import(texture_path);
        metadata.Name = asset_name;
        metadata.SpecializedConfig = texture_importer_detail::makeDefaultTextureConfig(texture_path);
        metadata.SpecializedConfig["SRGB"] = shouldUseSrgb(role);
        importer.serializeMetadata(metadata);
        created_metadata = true;
    }

    if (!metadata.Handle.isValid() || metadata.Type != AssetType::Texture || metadata.FilePath.empty()) {
        metadata = importer.import(texture_path);
        metadata.Name = asset_name;
        metadata.SpecializedConfig = texture_importer_detail::makeDefaultTextureConfig(texture_path);
        metadata.SpecializedConfig["SRGB"] = shouldUseSrgb(role);
        importer.serializeMetadata(metadata);
        created_metadata = true;
    }

    if (created_metadata) {
        ++result.CreatedTextureMetadata;
    }
    AssetDatabase::set(metadata.Handle, metadata);
    return metadata;
}

AssetHandle textureHandleForMaps(const std::filesystem::path& fbx_path,
                                 const std::string& material_name,
                                 TextureRole role,
                                 FbxModelAssetGenerator::GenerateResult& result,
                                 std::initializer_list<const ufbx_material_map*> maps)
{
    for (const ufbx_material_map* map : maps) {
        if (map == nullptr) {
            continue;
        }

        const auto texture_path = texturePathForMap(fbx_path, *map);
        if (!texture_path.has_value()) {
            continue;
        }

        const AssetMetadata texture_metadata =
            ensureTextureMetadata(*texture_path, textureAssetName(material_name, role), role, result);
        if (texture_metadata.Handle.isValid()) {
            return texture_metadata.Handle;
        }
    }

    return AssetHandle(0);
}

MaterialAssetDescriptor makeMaterialDescriptor(const std::filesystem::path& fbx_path,
                                               const ufbx_material& fbx_material,
                                               const std::string& material_name,
                                               FbxModelAssetGenerator::GenerateResult& result)
{
    MaterialAssetDescriptor descriptor = MaterialFactory::makeDefaultDescriptor(material_name);

    const glm::vec3 base_color =
        mapVec3(fbx_material.pbr.base_color, mapVec3(fbx_material.fbx.diffuse_color, glm::vec3(1.0f)));
    float alpha = 1.0f;
    if (fbx_material.pbr.opacity.has_value) {
        alpha = mapReal(fbx_material.pbr.opacity, 1.0f);
    } else if (fbx_material.fbx.transparency_factor.has_value) {
        alpha = 1.0f - mapReal(fbx_material.fbx.transparency_factor, 0.0f);
    }

    const float emission_factor =
        mapReal(fbx_material.pbr.emission_factor, mapReal(fbx_material.fbx.emission_factor, 1.0f));
    descriptor.Surface.BaseColorFactor = glm::vec4(base_color, alpha);
    descriptor.Surface.EmissiveFactor =
        mapVec3(fbx_material.pbr.emission_color, mapVec3(fbx_material.fbx.emission_color, glm::vec3(0.0f))) *
        emission_factor;
    descriptor.Surface.MetallicFactor = mapReal(fbx_material.pbr.metalness, 0.0f);
    descriptor.Surface.RoughnessFactor = mapReal(fbx_material.pbr.roughness, 1.0f);
    descriptor.Surface.NormalScale = mapReal(fbx_material.fbx.bump_factor, 1.0f);
    descriptor.Surface.BlendModeValue =
        alpha < 0.999f ? Material::BlendMode::Transparent : Material::BlendMode::Opaque;

    descriptor.Textures.BaseColor = textureHandleForMaps(fbx_path,
                                                         material_name,
                                                         TextureRole::BaseColor,
                                                         result,
                                                         {&fbx_material.pbr.base_color, &fbx_material.fbx.diffuse_color});
    descriptor.Textures.Normal = textureHandleForMaps(fbx_path,
                                                      material_name,
                                                      TextureRole::Normal,
                                                      result,
                                                      {&fbx_material.pbr.normal_map,
                                                       &fbx_material.fbx.normal_map,
                                                       &fbx_material.fbx.bump});
    descriptor.Textures.MetallicRoughness =
        textureHandleForMaps(fbx_path,
                             material_name,
                             TextureRole::MetallicRoughness,
                             result,
                             {&fbx_material.pbr.roughness,
                              &fbx_material.pbr.metalness,
                              &fbx_material.fbx.specular_factor});
    descriptor.Textures.Emissive = textureHandleForMaps(fbx_path,
                                                        material_name,
                                                        TextureRole::Emissive,
                                                        result,
                                                        {&fbx_material.pbr.emission_color,
                                                         &fbx_material.fbx.emission_color});
    descriptor.Textures.Occlusion = textureHandleForMaps(fbx_path,
                                                         material_name,
                                                         TextureRole::Occlusion,
                                                         result,
                                                         {&fbx_material.pbr.ambient_occlusion,
                                                          &fbx_material.fbx.ambient_color});

    return descriptor;
}

AssetMetadata ensureMaterialMetadata(const std::filesystem::path& material_path,
                                     const std::string& material_name,
                                     FbxModelAssetGenerator::GenerateResult& result)
{
    MaterialImporter importer;
    const std::filesystem::path meta_path = importer_detail::getMetadataPath(material_path);

    AssetMetadata metadata;
    bool created_metadata = false;
    if (std::filesystem::exists(meta_path)) {
        metadata = importer.deserializeMetadata(meta_path);
    } else {
        metadata = importer.import(material_path);
        metadata.Name = material_name;
        importer.serializeMetadata(metadata);
        created_metadata = true;
    }

    if (!metadata.Handle.isValid() || metadata.Type != AssetType::Material || metadata.FilePath.empty()) {
        metadata = importer.import(material_path);
        metadata.Name = material_name;
        importer.serializeMetadata(metadata);
        created_metadata = true;
    }

    if (created_metadata) {
        ++result.CreatedMaterialMetadata;
    }
    AssetDatabase::set(metadata.Handle, metadata);
    return metadata;
}

std::vector<GeneratedMaterial> ensureGeneratedMaterials(const ufbx_scene& scene,
                                                        const std::filesystem::path& fbx_path,
                                                        FbxModelAssetGenerator::GenerateResult& result)
{
    std::vector<GeneratedMaterial> generated_materials;
    generated_materials.reserve(scene.materials.count);

    const std::string model_stem = sanitizeFileStem(fbx_path.stem().string());
    const std::filesystem::path material_directory = fbx_path.parent_path() / "Materials";

    for (size_t material_index = 0; material_index < scene.materials.count; ++material_index) {
        const ufbx_material* fbx_material = scene.materials.data[material_index];
        if (fbx_material == nullptr) {
            generated_materials.push_back(GeneratedMaterial{
                .SourceMaterialIndex = static_cast<uint32_t>(material_index),
            });
            continue;
        }

        std::string fbx_material_name = toString(fbx_material->name);
        if (fbx_material_name.empty()) {
            fbx_material_name = "Material_" + std::to_string(material_index);
        }

        const std::string material_name = model_stem + "_" + sanitizeFileStem(fbx_material_name);
        const std::filesystem::path material_path =
            material_directory / (std::to_string(material_index) + "_" + material_name + ".lunamat");

        if (!std::filesystem::exists(material_path)) {
            const MaterialAssetDescriptor descriptor =
                makeMaterialDescriptor(fbx_path, *fbx_material, material_name, result);
            if (MaterialFactory::createMaterialFile(material_path, descriptor, false)) {
                ++result.CreatedMaterialFiles;
            }
        }

        const AssetMetadata material_metadata = ensureMaterialMetadata(material_path, material_name, result);
        generated_materials.push_back(GeneratedMaterial{
            .SourceMaterialIndex = static_cast<uint32_t>(material_index),
            .Handle = material_metadata.Handle,
            .FilePath = material_metadata.FilePath,
        });
    }

    return generated_materials;
}

AssetHandle materialHandleForMeshPart(const ufbx_scene& scene,
                                      const ufbx_mesh& mesh,
                                      uint32_t mesh_part_index,
                                      const std::vector<GeneratedMaterial>& generated_materials)
{
    if (mesh_part_index >= mesh.materials.count) {
        return AssetHandle(0);
    }

    const uint32_t material_index = findSceneMaterialIndex(&scene, mesh.materials.data[mesh_part_index]);
    if (material_index >= generated_materials.size()) {
        return AssetHandle(0);
    }

    return generated_materials[material_index].Handle;
}

std::vector<GeneratedMeshInfo> buildMeshInfos(const ufbx_scene& scene,
                                              const std::vector<GeneratedMaterial>& generated_materials,
                                              MeshInfoMap& mesh_info_by_mesh)
{
    std::vector<GeneratedMeshInfo> mesh_infos;
    mesh_infos.reserve(scene.meshes.count);

    uint32_t next_submesh = 0;
    for (size_t mesh_index = 0; mesh_index < scene.meshes.count; ++mesh_index) {
        const ufbx_mesh* mesh = scene.meshes.data[mesh_index];
        if (mesh == nullptr || !mesh->vertex_position.exists || mesh->num_triangles == 0) {
            continue;
        }

        GeneratedMeshInfo mesh_info;
        mesh_info.FirstSubmesh = next_submesh;

        bool emitted_material_part = false;
        for (size_t part_index = 0; part_index < mesh->material_parts.count; ++part_index) {
            const ufbx_mesh_part& mesh_part = mesh->material_parts.data[part_index];
            if (mesh_part.num_triangles == 0) {
                continue;
            }

            mesh_info.SubmeshMaterials.push_back(
                materialHandleForMeshPart(scene, *mesh, mesh_part.index, generated_materials));
            emitted_material_part = true;
        }

        if (!emitted_material_part) {
            AssetHandle material_handle(0);
            if (mesh->materials.count == 1) {
                const uint32_t material_index = findSceneMaterialIndex(&scene, mesh->materials.data[0]);
                if (material_index < generated_materials.size()) {
                    material_handle = generated_materials[material_index].Handle;
                }
            }
            mesh_info.SubmeshMaterials.push_back(material_handle);
        }

        mesh_info.SubmeshCount = static_cast<uint32_t>(mesh_info.SubmeshMaterials.size());
        next_submesh += mesh_info.SubmeshCount;
        mesh_info_by_mesh[mesh] = mesh_info;
        mesh_infos.push_back(std::move(mesh_info));
    }

    return mesh_infos;
}

std::vector<GeneratedNode> buildGeneratedNodes(const ufbx_scene& scene,
                                               const std::filesystem::path& fbx_path,
                                               AssetHandle mesh_handle,
                                               const MeshInfoMap& mesh_info_by_mesh)
{
    std::vector<GeneratedNode> nodes;
    if (scene.root_node == nullptr) {
        return nodes;
    }

    std::vector<const ufbx_node*> root_nodes;
    for (size_t child_index = 0; child_index < scene.root_node->children.count; ++child_index) {
        root_nodes.push_back(scene.root_node->children.data[child_index]);
    }

    if (root_nodes.empty()) {
        for (size_t node_index = 0; node_index < scene.nodes.count; ++node_index) {
            const ufbx_node* node = scene.nodes.data[node_index];
            if (node != nullptr && node->parent == nullptr) {
                root_nodes.push_back(node);
            }
        }
    }

    if (root_nodes.empty()) {
        for (size_t node_index = 0; node_index < scene.nodes.count; ++node_index) {
            const ufbx_node* node = scene.nodes.data[node_index];
            if (node != nullptr) {
                root_nodes.push_back(node);
            }
        }
    }

    std::unordered_set<const ufbx_node*> visited;
    auto append_node = [&](auto&& self, const ufbx_node* node, int32_t parent_index) -> void {
        if (node == nullptr || visited.contains(node)) {
            return;
        }
        visited.insert(node);

        std::string node_name = sanitizeFileStem(fbx_path.stem().string()) + "_Node_" +
                                std::to_string(node->typed_id);
        const std::string node_label = toString(node->name);
        if (!node_label.empty()) {
            node_name = node_label;
        }

        nodes.push_back(makeGeneratedNode(node_name, parent_index, node->local_transform));
        const int32_t current_index = static_cast<int32_t>(nodes.size() - 1);

        if (const auto mesh_it = mesh_info_by_mesh.find(node->mesh); mesh_it != mesh_info_by_mesh.end()) {
            if (!isIdentityTransform(node->geometry_transform)) {
                nodes.push_back(makeGeneratedNode(node_name + "_Geometry", current_index, node->geometry_transform));
                attachMeshInfo(nodes.back(), mesh_it->second, mesh_handle);
            } else {
                attachMeshInfo(nodes.back(), mesh_it->second, mesh_handle);
            }
        }

        for (size_t child_index = 0; child_index < node->children.count; ++child_index) {
            self(self, node->children.data[child_index], current_index);
        }
    };

    for (const ufbx_node* root_node : root_nodes) {
        append_node(append_node, root_node, -1);
    }

    if (nodes.empty()) {
        GeneratedNode fallback_node;
        fallback_node.Name = sanitizeFileStem(fbx_path.stem().string());
        fallback_node.MeshHandle = mesh_handle;
        fallback_node.FirstSubmesh = 0;
        fallback_node.SubmeshCount = 0;
        for (size_t mesh_index = 0; mesh_index < scene.meshes.count; ++mesh_index) {
            const ufbx_mesh* mesh = scene.meshes.data[mesh_index];
            if (mesh == nullptr) {
                continue;
            }
            if (const auto mesh_it = mesh_info_by_mesh.find(mesh); mesh_it != mesh_info_by_mesh.end()) {
                fallback_node.SubmeshMaterials.insert(fallback_node.SubmeshMaterials.end(),
                                                      mesh_it->second.SubmeshMaterials.begin(),
                                                      mesh_it->second.SubmeshMaterials.end());
                fallback_node.SubmeshCount += mesh_it->second.SubmeshCount;
            }
        }
        nodes.push_back(std::move(fallback_node));
    }

    return nodes;
}

bool isLegacyGeneratedModelFile(const std::filesystem::path& model_path, const AssetMetadata& mesh_metadata)
{
    if (!std::filesystem::exists(model_path)) {
        return false;
    }

    try {
        const YAML::Node data = YAML::LoadFile(model_path.string());
        const YAML::Node model_node = data["Model"] ? data["Model"] : data;
        if (!model_node || !model_node["SourceMesh"]) {
            return false;
        }

        if (readHandle(model_node["SourceMesh"]) != mesh_metadata.Handle) {
            return false;
        }

        const YAML::Node nodes = model_node["Nodes"];
        if (!nodes || !nodes.IsSequence() || nodes.size() != 1) {
            return false;
        }

        const YAML::Node node = nodes[0];
        if (!node) {
            return false;
        }

        const AssetHandle node_mesh_handle = node["Mesh"] ? readHandle(node["Mesh"]) : readHandle(node["MeshHandle"]);
        if (node_mesh_handle != mesh_metadata.Handle) {
            return false;
        }

        if (node["FirstSubmesh"] || node["SubmeshCount"]) {
            return false;
        }

        return true;
    } catch (const YAML::Exception&) {
        return false;
    }
}

bool writeModelFile(const std::filesystem::path& model_path,
                    const AssetMetadata& mesh_metadata,
                    const std::vector<GeneratedMaterial>& generated_materials,
                    const std::vector<GeneratedNode>& nodes)
{
    if (model_path.empty() || !mesh_metadata.Handle.isValid() || nodes.empty()) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(model_path.parent_path(), ec);
    if (ec) {
        return false;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Model" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Name" << YAML::Value << model_path.stem().string();
    out << YAML::Key << "Source" << YAML::Value << mesh_metadata.FilePath.generic_string();
    out << YAML::Key << "SourceMesh" << YAML::Value;
    emitHandle(out, mesh_metadata.Handle);

    out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
    for (const GeneratedMaterial& material : generated_materials) {
        out << YAML::BeginMap;
        out << YAML::Key << "SourceMaterialIndex" << YAML::Value << material.SourceMaterialIndex;
        out << YAML::Key << "Handle" << YAML::Value;
        emitHandle(out, material.Handle);
        out << YAML::Key << "FilePath" << YAML::Value << material.FilePath.generic_string();
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
    for (const GeneratedNode& node : nodes) {
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << node.Name;
        out << YAML::Key << "Parent" << YAML::Value << node.Parent;
        out << YAML::Key << "Translation" << YAML::Value;
        emitVec3(out, node.Translation);
        out << YAML::Key << "Rotation" << YAML::Value;
        emitVec3(out, node.Rotation);
        out << YAML::Key << "Scale" << YAML::Value;
        emitVec3(out, node.Scale);
        if (node.MeshHandle.isValid()) {
            out << YAML::Key << "Mesh" << YAML::Value;
            emitHandle(out, node.MeshHandle);
            out << YAML::Key << "FirstSubmesh" << YAML::Value << node.FirstSubmesh;
            out << YAML::Key << "SubmeshCount" << YAML::Value << node.SubmeshCount;
            out << YAML::Key << "SubmeshMaterials" << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (const AssetHandle material_handle : node.SubmeshMaterials) {
                emitHandle(out, material_handle);
            }
            out << YAML::EndSeq;
        }
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::EndMap;
    out << YAML::EndMap;

    if (!out.good()) {
        return false;
    }

    std::ofstream stream(model_path, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }

    stream << out.c_str();
    return stream.good();
}

AssetMetadata ensureModelMetadata(const std::filesystem::path& model_path,
                                  FbxModelAssetGenerator::GenerateResult& result)
{
    ModelImporter importer;
    const std::filesystem::path meta_path = importer_detail::getMetadataPath(model_path);

    AssetMetadata metadata;
    bool created_metadata = false;
    if (std::filesystem::exists(meta_path)) {
        metadata = importer.deserializeMetadata(meta_path);
    } else {
        metadata = importer.import(model_path);
        importer.serializeMetadata(metadata);
        created_metadata = true;
    }

    if (!metadata.Handle.isValid() || metadata.Type != AssetType::Model || metadata.FilePath.empty()) {
        metadata = importer.import(model_path);
        importer.serializeMetadata(metadata);
        created_metadata = true;
    }

    if (created_metadata) {
        result.CreatedModelMetadata = true;
    }
    AssetDatabase::set(metadata.Handle, metadata);
    return metadata;
}

} // namespace luna::fbx_model_asset_generator_detail

namespace luna {

FbxModelAssetGenerator::GenerateResult
    FbxModelAssetGenerator::generateCompanionAssets(const std::filesystem::path& fbx_path,
                                                    const AssetMetadata& mesh_metadata)
{
    using namespace fbx_model_asset_generator_detail;

    GenerateResult result{};
    if (fbx_path.empty() || mesh_metadata.Type != AssetType::Mesh || !mesh_metadata.Handle.isValid()) {
        return result;
    }

    FbxScenePtr scene = loadFbxForCompanionGeneration(fbx_path);
    if (!scene) {
        LUNA_CORE_WARN("Failed to parse FBX companion assets for '{}'", fbx_path.string());
        return result;
    }

    const std::filesystem::path model_path = fbx_path.parent_path() / (fbx_path.stem().string() + ".lmodel");
    const std::vector<GeneratedMaterial> generated_materials =
        ensureGeneratedMaterials(*scene, fbx_path, result);
    MeshInfoMap mesh_info_by_mesh;
    (void) buildMeshInfos(*scene, generated_materials, mesh_info_by_mesh);
    const std::vector<GeneratedNode> nodes = buildGeneratedNodes(*scene, fbx_path, mesh_metadata.Handle, mesh_info_by_mesh);

    if (!std::filesystem::exists(model_path) || isLegacyGeneratedModelFile(model_path, mesh_metadata)) {
        if (writeModelFile(model_path, mesh_metadata, generated_materials, nodes)) {
            result.CreatedModelFile = true;
        } else {
            LUNA_CORE_WARN("Failed to write generated model asset '{}'", model_path.string());
            return result;
        }
    }

    ensureModelMetadata(model_path, result);
    result.Success = true;
    return result;
}

} // namespace luna
