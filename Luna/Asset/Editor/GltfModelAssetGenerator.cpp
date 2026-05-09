#include "Asset/Editor/GltfModelAssetGenerator.h"

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
#include <glm/geometric.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <yaml-cpp/yaml.h>

namespace luna::gltf_model_asset_generator_detail {

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

glm::vec3 toVec3(const fastgltf::math::fvec3& value)
{
    return {static_cast<float>(value.x()), static_cast<float>(value.y()), static_cast<float>(value.z())};
}

glm::quat toQuat(const fastgltf::math::fquat& value)
{
    return glm::quat(static_cast<float>(value.w()),
                     static_cast<float>(value.x()),
                     static_cast<float>(value.y()),
                     static_cast<float>(value.z()));
}

glm::vec3 toEulerRadians(const fastgltf::math::fquat& value)
{
    return glm::eulerAngles(glm::normalize(toQuat(value)));
}

AssetHandle readHandle(const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        return AssetHandle(0);
    }

    return AssetHandle(node.as<uint64_t>());
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

fastgltf::Extensions supportedGltfExtensions()
{
    return fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_mesh_quantization |
           fastgltf::Extensions::MSFT_texture_dds | fastgltf::Extensions::EXT_texture_webp |
           fastgltf::Extensions::KHR_materials_unlit |
           fastgltf::Extensions::KHR_materials_emissive_strength;
}

fastgltf::Category companionParseCategories()
{
    return fastgltf::Category::Buffers | fastgltf::Category::BufferViews | fastgltf::Category::Accessors |
           fastgltf::Category::Images | fastgltf::Category::Samplers | fastgltf::Category::Textures |
           fastgltf::Category::Materials | fastgltf::Category::Meshes | fastgltf::Category::Nodes |
           fastgltf::Category::Scenes;
}

std::optional<fastgltf::Asset> loadGltfForCompanionGeneration(const std::filesystem::path& gltf_path)
{
    auto data = fastgltf::GltfDataBuffer::FromPath(gltf_path);
    if (data.error() != fastgltf::Error::None) {
        return std::nullopt;
    }

    fastgltf::Parser parser(supportedGltfExtensions());
    auto asset = parser.loadGltf(data.get(),
                                 gltf_path.parent_path(),
                                 fastgltf::Options::DecomposeNodeMatrices,
                                 companionParseCategories());
    if (asset.error() != fastgltf::Error::None) {
        return std::nullopt;
    }

    return std::move(asset.get());
}

std::optional<size_t> imageIndexForTexture(const fastgltf::Asset& asset, size_t texture_index)
{
    if (texture_index >= asset.textures.size()) {
        return std::nullopt;
    }

    const fastgltf::Texture& texture = asset.textures[texture_index];
    if (texture.imageIndex.has_value()) {
        return *texture.imageIndex;
    }
    if (texture.ddsImageIndex.has_value()) {
        return *texture.ddsImageIndex;
    }
    if (texture.basisuImageIndex.has_value()) {
        return *texture.basisuImageIndex;
    }
    if (texture.webpImageIndex.has_value()) {
        return *texture.webpImageIndex;
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> externalImagePath(const fastgltf::Asset& asset,
                                                       const std::filesystem::path& gltf_path,
                                                       size_t texture_index)
{
    const auto image_index = imageIndexForTexture(asset, texture_index);
    if (!image_index.has_value() || *image_index >= asset.images.size()) {
        return std::nullopt;
    }

    const fastgltf::Image& image = asset.images[*image_index];
    const auto* uri_source = std::get_if<fastgltf::sources::URI>(&image.data);
    if (uri_source == nullptr) {
        return std::nullopt;
    }

    const std::filesystem::path image_path = uri_source->uri.fspath();
    if (image_path.empty()) {
        return std::nullopt;
    }

    return (gltf_path.parent_path() / image_path).lexically_normal();
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
                                    GltfModelAssetGenerator::GenerateResult& result)
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

template <typename OptionalTextureInfo>
AssetHandle textureHandleForInfo(const fastgltf::Asset& asset,
                                 const std::filesystem::path& gltf_path,
                                 const std::string& material_name,
                                 const OptionalTextureInfo& texture_info,
                                 TextureRole role,
                                 GltfModelAssetGenerator::GenerateResult& result)
{
    if (!texture_info.has_value()) {
        return AssetHandle(0);
    }

    const auto image_path = externalImagePath(asset, gltf_path, texture_info->textureIndex);
    if (!image_path.has_value()) {
        return AssetHandle(0);
    }

    const AssetMetadata texture_metadata =
        ensureTextureMetadata(*image_path, textureAssetName(material_name, role), role, result);
    return texture_metadata.Handle;
}

Material::BlendMode toBlendMode(fastgltf::AlphaMode alpha_mode)
{
    switch (alpha_mode) {
        case fastgltf::AlphaMode::Mask:
            return Material::BlendMode::Masked;
        case fastgltf::AlphaMode::Blend:
            return Material::BlendMode::Transparent;
        case fastgltf::AlphaMode::Opaque:
        default:
            return Material::BlendMode::Opaque;
    }
}

MaterialAssetDescriptor makeMaterialDescriptor(const fastgltf::Asset& asset,
                                               const std::filesystem::path& gltf_path,
                                               const fastgltf::Material& gltf_material,
                                               const std::string& material_name,
                                               GltfModelAssetGenerator::GenerateResult& result)
{
    MaterialAssetDescriptor descriptor = MaterialFactory::makeDefaultDescriptor(material_name);

    descriptor.Surface.BaseColorFactor = glm::vec4(gltf_material.pbrData.baseColorFactor[0],
                                                   gltf_material.pbrData.baseColorFactor[1],
                                                   gltf_material.pbrData.baseColorFactor[2],
                                                   gltf_material.pbrData.baseColorFactor[3]);
    descriptor.Surface.EmissiveFactor = glm::vec3(gltf_material.emissiveFactor[0],
                                                  gltf_material.emissiveFactor[1],
                                                  gltf_material.emissiveFactor[2]) *
                                        static_cast<float>(gltf_material.emissiveStrength);
    descriptor.Surface.MetallicFactor = static_cast<float>(gltf_material.pbrData.metallicFactor);
    descriptor.Surface.RoughnessFactor = static_cast<float>(gltf_material.pbrData.roughnessFactor);
    descriptor.Surface.AlphaCutoff = static_cast<float>(gltf_material.alphaCutoff);
    descriptor.Surface.DoubleSided = gltf_material.doubleSided;
    descriptor.Surface.Unlit = gltf_material.unlit;
    descriptor.Surface.BlendModeValue = toBlendMode(gltf_material.alphaMode);

    if (gltf_material.normalTexture.has_value()) {
        descriptor.Surface.NormalScale = static_cast<float>(gltf_material.normalTexture->scale);
    }
    if (gltf_material.occlusionTexture.has_value()) {
        descriptor.Surface.OcclusionStrength = static_cast<float>(gltf_material.occlusionTexture->strength);
    }

    descriptor.Textures.BaseColor = textureHandleForInfo(asset,
                                                         gltf_path,
                                                         material_name,
                                                         gltf_material.pbrData.baseColorTexture,
                                                         TextureRole::BaseColor,
                                                         result);
    descriptor.Textures.Normal = textureHandleForInfo(asset,
                                                      gltf_path,
                                                      material_name,
                                                      gltf_material.normalTexture,
                                                      TextureRole::Normal,
                                                      result);
    descriptor.Textures.MetallicRoughness =
        textureHandleForInfo(asset,
                             gltf_path,
                             material_name,
                             gltf_material.pbrData.metallicRoughnessTexture,
                             TextureRole::MetallicRoughness,
                             result);
    descriptor.Textures.Emissive = textureHandleForInfo(asset,
                                                        gltf_path,
                                                        material_name,
                                                        gltf_material.emissiveTexture,
                                                        TextureRole::Emissive,
                                                        result);
    descriptor.Textures.Occlusion = textureHandleForInfo(asset,
                                                         gltf_path,
                                                         material_name,
                                                         gltf_material.occlusionTexture,
                                                         TextureRole::Occlusion,
                                                         result);

    return descriptor;
}

AssetMetadata ensureMaterialMetadata(const std::filesystem::path& material_path,
                                     const std::string& material_name,
                                     GltfModelAssetGenerator::GenerateResult& result)
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

std::vector<GeneratedMaterial> ensureGeneratedMaterials(const fastgltf::Asset& asset,
                                                        const std::filesystem::path& gltf_path,
                                                        GltfModelAssetGenerator::GenerateResult& result)
{
    std::vector<GeneratedMaterial> generated_materials;
    generated_materials.reserve(asset.materials.size());

    const std::string model_stem = sanitizeFileStem(gltf_path.stem().string());
    const std::filesystem::path material_directory = gltf_path.parent_path() / "Materials";

    for (size_t material_index = 0; material_index < asset.materials.size(); ++material_index) {
        const fastgltf::Material& gltf_material = asset.materials[material_index];
        const std::string gltf_material_name =
            gltf_material.name.empty() ? "Material_" + std::to_string(material_index)
                                       : std::string(gltf_material.name);
        const std::string material_name = model_stem + "_" + sanitizeFileStem(gltf_material_name);
        const std::filesystem::path material_path =
            material_directory / (std::to_string(material_index) + "_" + material_name + ".lunamat");

        if (!std::filesystem::exists(material_path)) {
            const MaterialAssetDescriptor descriptor =
                makeMaterialDescriptor(asset, gltf_path, gltf_material, material_name, result);
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

GeneratedMeshInfo buildMeshInfo(const fastgltf::Mesh& mesh, const std::vector<GeneratedMaterial>& generated_materials)
{
    GeneratedMeshInfo mesh_info;

    for (const auto& primitive : mesh.primitives) {
        if (primitive.type != fastgltf::PrimitiveType::Triangles) {
            continue;
        }

        if (primitive.findAttribute("POSITION") == primitive.attributes.end()) {
            continue;
        }

        AssetHandle material_handle(0);
        if (primitive.materialIndex.has_value()) {
            const size_t material_index = *primitive.materialIndex;
            if (material_index < generated_materials.size()) {
                material_handle = generated_materials[material_index].Handle;
            }
        }

        mesh_info.SubmeshMaterials.push_back(material_handle);
    }

    mesh_info.SubmeshCount = static_cast<uint32_t>(mesh_info.SubmeshMaterials.size());
    return mesh_info;
}

std::vector<GeneratedMeshInfo> buildMeshInfos(const fastgltf::Asset& asset,
                                              const std::vector<GeneratedMaterial>& generated_materials)
{
    std::vector<GeneratedMeshInfo> mesh_infos;
    mesh_infos.reserve(asset.meshes.size());

    uint32_t next_submesh = 0;
    for (const auto& mesh : asset.meshes) {
        GeneratedMeshInfo mesh_info = buildMeshInfo(mesh, generated_materials);
        mesh_info.FirstSubmesh = next_submesh;
        next_submesh += mesh_info.SubmeshCount;
        mesh_infos.push_back(std::move(mesh_info));
    }

    return mesh_infos;
}

GeneratedNode makeGeneratedNode(std::string name,
                                int32_t parent,
                                glm::vec3 translation,
                                glm::vec3 rotation,
                                glm::vec3 scale)
{
    GeneratedNode node;
    node.Name = std::move(name);
    node.Parent = parent;
    node.Translation = translation;
    node.Rotation = rotation;
    node.Scale = scale;
    return node;
}

GeneratedNode makeGeneratedNode(const std::string& name, int32_t parent, const fastgltf::Node& node)
{
    GeneratedNode generated_node = makeGeneratedNode(name, parent, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));

    if (const auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
        generated_node.Translation = toVec3(trs->translation);
        generated_node.Rotation = toEulerRadians(trs->rotation);
        generated_node.Scale = toVec3(trs->scale);
    } else if (const auto* matrix = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
        fastgltf::math::fvec3 translation{};
        fastgltf::math::fvec3 scale{};
        fastgltf::math::fquat rotation{};
        fastgltf::math::decomposeTransformMatrix(*matrix, scale, rotation, translation);
        generated_node.Translation = toVec3(translation);
        generated_node.Rotation = toEulerRadians(rotation);
        generated_node.Scale = toVec3(scale);
    }

    return generated_node;
}

void attachMeshInfo(GeneratedNode& node, const GeneratedMeshInfo& mesh_info, const AssetHandle mesh_handle)
{
    if (mesh_info.SubmeshCount == 0) {
        return;
    }

    node.MeshHandle = mesh_handle;
    node.FirstSubmesh = mesh_info.FirstSubmesh;
    node.SubmeshCount = mesh_info.SubmeshCount;
    node.SubmeshMaterials = mesh_info.SubmeshMaterials;
}

std::vector<GeneratedNode> buildGeneratedNodes(const fastgltf::Asset& asset,
                                               const std::filesystem::path& gltf_path,
                                               const AssetHandle mesh_handle,
                                               const std::vector<GeneratedMeshInfo>& mesh_infos)
{
    std::vector<GeneratedNode> nodes;
    if (asset.nodes.empty()) {
        GeneratedNode fallback_node;
        fallback_node.Name = sanitizeFileStem(gltf_path.stem().string());
        fallback_node.MeshHandle = mesh_handle;
        fallback_node.FirstSubmesh = 0;
        fallback_node.SubmeshCount = 0;
        for (const auto& mesh_info : mesh_infos) {
            fallback_node.SubmeshMaterials.insert(fallback_node.SubmeshMaterials.end(),
                                                  mesh_info.SubmeshMaterials.begin(),
                                                  mesh_info.SubmeshMaterials.end());
            fallback_node.SubmeshCount += mesh_info.SubmeshCount;
        }
        nodes.push_back(std::move(fallback_node));
        return nodes;
    }

    std::unordered_set<size_t> child_nodes;
    for (const auto& node : asset.nodes) {
        for (const size_t child_index : node.children) {
            child_nodes.insert(child_index);
        }
    }

    std::vector<size_t> root_node_indices;
    if (!asset.scenes.empty()) {
        size_t scene_index = asset.defaultScene.has_value() ? *asset.defaultScene : 0;
        if (scene_index < asset.scenes.size()) {
            const auto& scene = asset.scenes[scene_index];
            root_node_indices.assign(scene.nodeIndices.begin(), scene.nodeIndices.end());
        }
    }

    if (root_node_indices.empty()) {
        for (size_t node_index = 0; node_index < asset.nodes.size(); ++node_index) {
            if (!child_nodes.contains(node_index)) {
                root_node_indices.push_back(node_index);
            }
        }
    }

    if (root_node_indices.empty()) {
        for (size_t node_index = 0; node_index < asset.nodes.size(); ++node_index) {
            root_node_indices.push_back(node_index);
        }
    }

    std::unordered_set<size_t> visited;
    auto append_node = [&](auto&& self, size_t node_index, int32_t parent_index) -> void {
        if (node_index >= asset.nodes.size() || visited.contains(node_index)) {
            return;
        }
        visited.insert(node_index);

        const fastgltf::Node& source_node = asset.nodes[node_index];
        std::string node_name = source_node.name.empty() ? sanitizeFileStem(gltf_path.stem().string()) + "_Node_" +
                                                               std::to_string(node_index)
                                                         : std::string(source_node.name);

        nodes.push_back(makeGeneratedNode(node_name, parent_index, source_node));
        const int32_t current_index = static_cast<int32_t>(nodes.size() - 1);

        if (source_node.meshIndex.has_value() && *source_node.meshIndex < mesh_infos.size()) {
            attachMeshInfo(nodes.back(), mesh_infos[*source_node.meshIndex], mesh_handle);
        }

        for (const size_t child_index : source_node.children) {
            self(self, child_index, current_index);
        }
    };

    for (const size_t root_node_index : root_node_indices) {
        append_node(append_node, root_node_index, -1);
    }

    if (nodes.empty()) {
        GeneratedNode fallback_node;
        fallback_node.Name = sanitizeFileStem(gltf_path.stem().string());
        fallback_node.MeshHandle = mesh_handle;
        fallback_node.FirstSubmesh = 0;
        fallback_node.SubmeshCount = 0;
        for (const auto& mesh_info : mesh_infos) {
            fallback_node.SubmeshMaterials.insert(fallback_node.SubmeshMaterials.end(),
                                                  mesh_info.SubmeshMaterials.begin(),
                                                  mesh_info.SubmeshMaterials.end());
            fallback_node.SubmeshCount += mesh_info.SubmeshCount;
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
                                  GltfModelAssetGenerator::GenerateResult& result)
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

} // namespace luna::gltf_model_asset_generator_detail

namespace luna {

GltfModelAssetGenerator::GenerateResult
    GltfModelAssetGenerator::generateCompanionAssets(const std::filesystem::path& gltf_path,
                                                     const AssetMetadata& mesh_metadata)
{
    using namespace gltf_model_asset_generator_detail;

    GenerateResult result{};
    if (gltf_path.empty() || mesh_metadata.Type != AssetType::Mesh || !mesh_metadata.Handle.isValid()) {
        return result;
    }

    const auto asset = loadGltfForCompanionGeneration(gltf_path);
    if (!asset.has_value()) {
        LUNA_CORE_WARN("Failed to parse glTF companion assets for '{}'", gltf_path.string());
        return result;
    }

    const std::filesystem::path model_path = gltf_path.parent_path() / (gltf_path.stem().string() + ".lmodel");
    const std::vector<GeneratedMaterial> generated_materials =
        ensureGeneratedMaterials(*asset, gltf_path, result);
    const std::vector<GeneratedMeshInfo> mesh_infos = buildMeshInfos(*asset, generated_materials);
    const std::vector<GeneratedNode> nodes = buildGeneratedNodes(*asset, gltf_path, mesh_metadata.Handle, mesh_infos);

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
