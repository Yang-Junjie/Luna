#pragma once

#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Loader.h"
#include "Project/ProjectManager.h"
#include "Renderer/Material.h"
#include "TextureLoader.h"
#include "yaml-cpp/yaml.h"

#include <cctype>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <memory>
#include <string>

#if defined(_MSC_VER)
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#endif

#include "third_party/tinyobjloader/tiny_obj_loader.h"

namespace luna::material_loader_detail {

inline Material::BlendMode parseBlendMode(const std::string& value)
{
    if (value == "Masked") {
        return Material::BlendMode::Masked;
    }
    if (value == "Transparent") {
        return Material::BlendMode::Transparent;
    }
    if (value == "Additive") {
        return Material::BlendMode::Additive;
    }
    return Material::BlendMode::Opaque;
}

inline glm::vec3 readVec3(const YAML::Node& node, const glm::vec3& default_value)
{
    if (!node || !node.IsSequence() || node.size() < 3) {
        return default_value;
    }

    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>()};
}

inline glm::vec4 readVec4(const YAML::Node& node, const glm::vec4& default_value)
{
    if (!node || !node.IsSequence() || node.size() < 4) {
        return default_value;
    }

    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>()};
}

inline std::shared_ptr<Texture> loadTextureRelative(const std::filesystem::path& base_dir,
                                                    const YAML::Node& textures_node,
                                                    const char* key,
                                                    const std::string& debug_name)
{
    if (!textures_node || !textures_node[key]) {
        return {};
    }

    const YAML::Node texture_node = textures_node[key];

    if (texture_node.IsScalar()) {
        const std::string scalar = texture_node.Scalar();

        uint64_t handle_value = 0;
        const char* begin = scalar.data();
        const char* end = begin + scalar.size();
        const auto [ptr, ec] = std::from_chars(begin, end, handle_value);
        if (ec == std::errc{} && ptr == end) {
            const AssetHandle texture_handle(handle_value);
            if (texture_handle.isValid()) {
                return AssetManager::get().loadAssetAs<Texture>(texture_handle);
            }

            return {};
        }

        const auto path = (base_dir / scalar).lexically_normal();
        return TextureLoader::loadFromFile(path, debug_name);
    }

    return {};
}

inline std::shared_ptr<Material> loadFromYaml(const std::filesystem::path& path, std::string asset_name)
{
    try {
        const YAML::Node data = YAML::LoadFile(path.string());
        const YAML::Node material_node = data["Material"] ? data["Material"] : data;
        if (!material_node) {
            return {};
        }

        const YAML::Node surface_node = material_node["Surface"] ? material_node["Surface"] : material_node;
        const YAML::Node textures_node = material_node["Textures"];

        if (asset_name.empty()) {
            asset_name = material_node["Name"] ? material_node["Name"].as<std::string>() : path.stem().string();
        }

        Material::TextureSet textures;
        textures.BaseColor =
            loadTextureRelative(path.parent_path(), textures_node, "BaseColor", asset_name + "_BaseColor");
        textures.Normal = loadTextureRelative(path.parent_path(), textures_node, "Normal", asset_name + "_Normal");
        textures.MetallicRoughness = loadTextureRelative(
            path.parent_path(), textures_node, "MetallicRoughness", asset_name + "_MetallicRoughness");
        textures.Emissive =
            loadTextureRelative(path.parent_path(), textures_node, "Emissive", asset_name + "_Emissive");
        textures.Occlusion =
            loadTextureRelative(path.parent_path(), textures_node, "Occlusion", asset_name + "_Occlusion");

        Material::SurfaceProperties surface;
        surface.BaseColorFactor = readVec4(surface_node["BaseColorFactor"], surface.BaseColorFactor);
        surface.EmissiveFactor = readVec3(surface_node["EmissiveFactor"], surface.EmissiveFactor);
        surface.MetallicFactor =
            surface_node["MetallicFactor"] ? surface_node["MetallicFactor"].as<float>() : surface.MetallicFactor;
        surface.RoughnessFactor =
            surface_node["RoughnessFactor"] ? surface_node["RoughnessFactor"].as<float>() : surface.RoughnessFactor;
        surface.NormalScale =
            surface_node["NormalScale"] ? surface_node["NormalScale"].as<float>() : surface.NormalScale;
        surface.OcclusionStrength = surface_node["OcclusionStrength"] ? surface_node["OcclusionStrength"].as<float>()
                                                                      : surface.OcclusionStrength;
        surface.AlphaCutoff =
            surface_node["AlphaCutoff"] ? surface_node["AlphaCutoff"].as<float>() : surface.AlphaCutoff;
        surface.DoubleSided =
            surface_node["DoubleSided"] ? surface_node["DoubleSided"].as<bool>() : surface.DoubleSided;
        surface.Unlit = surface_node["Unlit"] ? surface_node["Unlit"].as<bool>() : surface.Unlit;

        const YAML::Node blend_mode_node =
            surface_node["BlendMode"] ? surface_node["BlendMode"] : material_node["BlendMode"];
        if (blend_mode_node) {
            surface.BlendModeValue = parseBlendMode(blend_mode_node.as<std::string>());
        }

        return Material::create(std::move(asset_name), std::move(textures), surface);
    } catch (const YAML::Exception& error) {
        const char* message = error.what();
        LUNA_CORE_WARN("{}", message);
        return {};
    }
}

inline std::shared_ptr<Material> loadFromMtl(const std::filesystem::path& path, std::string asset_name)
{
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return {};
    }

    std::map<std::string, int> material_map;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;
    tinyobj::LoadMtl(&material_map, &materials, &stream, &warning, &error);

    if (materials.empty()) {
        return {};
    }

    const tinyobj::material_t* selected_material = &materials.front();
    if (!asset_name.empty()) {
        for (const auto& material : materials) {
            if (material.name == asset_name) {
                selected_material = &material;
                break;
            }
        }
    }

    if (asset_name.empty()) {
        asset_name = selected_material->name.empty() ? path.stem().string() : selected_material->name;
    }

    auto load_texture = [&](const std::string& relative_path, const std::string& suffix) -> std::shared_ptr<Texture> {
        if (relative_path.empty()) {
            return {};
        }

        return TextureLoader::loadFromFile((path.parent_path() / relative_path).lexically_normal(),
                                           asset_name + suffix);
    };

    Material::TextureSet textures;
    textures.BaseColor = load_texture(selected_material->diffuse_texname, "_BaseColor");
    textures.Normal = load_texture(selected_material->normal_texname, "_Normal");
    textures.MetallicRoughness =
        load_texture(!selected_material->roughness_texname.empty() ? selected_material->roughness_texname
                                                                   : selected_material->metallic_texname,
                     "_MetallicRoughness");
    textures.Emissive = load_texture(selected_material->emissive_texname, "_Emissive");
    textures.Occlusion = load_texture(selected_material->ambient_texname, "_Occlusion");

    Material::SurfaceProperties surface;
    surface.BaseColorFactor = glm::vec4(selected_material->diffuse[0],
                                        selected_material->diffuse[1],
                                        selected_material->diffuse[2],
                                        selected_material->dissolve);
    surface.EmissiveFactor =
        glm::vec3(selected_material->emission[0], selected_material->emission[1], selected_material->emission[2]);
    surface.MetallicFactor = selected_material->metallic;
    surface.RoughnessFactor = selected_material->roughness > 0.0f ? selected_material->roughness : 1.0f;
    surface.BlendModeValue =
        selected_material->dissolve < 0.999f ? Material::BlendMode::Transparent : Material::BlendMode::Opaque;

    return Material::create(std::move(asset_name), std::move(textures), surface);
}

} // namespace luna::material_loader_detail

namespace luna {

class MaterialLoader final : public Loader {
public:
    std::shared_ptr<Asset> load(const AssetMetadata& meta_data) override
    {
        const auto project_root_path = ProjectManager::instance().getProjectRootPath();
        if (!project_root_path) {
            return {};
        }

        return loadFromFile(*project_root_path / meta_data.FilePath, meta_data.Name);
    }

    static std::shared_ptr<Material> loadFromFile(const std::filesystem::path& path, std::string asset_name = {})
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (extension == ".mtl") {
            return material_loader_detail::loadFromMtl(path, std::move(asset_name));
        }
        if (extension == ".material" || extension == ".lmat" || extension == ".lunamat") {
            return material_loader_detail::loadFromYaml(path, std::move(asset_name));
        }

        return {};
    }
};

} // namespace luna
