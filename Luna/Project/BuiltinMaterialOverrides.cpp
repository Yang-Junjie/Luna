#include "Project/BuiltinMaterialOverrides.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Renderer/Material.h"

#include <filesystem>
#include <fstream>

#include <yaml-cpp/yaml.h>

namespace luna {
namespace {

constexpr const char* kSettingsDirectory = "ProjectSettings";
constexpr const char* kOverridesFile = "BuiltinMaterialOverrides.yaml";
constexpr const char* kRootKey = "BuiltinMaterialOverrides";

void emitVec3(YAML::Emitter& out, const glm::vec3& value)
{
    out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
}

void emitVec4(YAML::Emitter& out, const glm::vec4& value)
{
    out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
}

glm::vec3 readVec3(const YAML::Node& node, const glm::vec3& fallback)
{
    if (!node || !node.IsSequence() || node.size() < 3) {
        return fallback;
    }
    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>()};
}

glm::vec4 readVec4(const YAML::Node& node, const glm::vec4& fallback)
{
    if (!node || !node.IsSequence() || node.size() < 4) {
        return fallback;
    }
    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>()};
}

bool applyOverride(AssetHandle handle, const YAML::Node& node)
{
    if (!BuiltinAssets::isBuiltinMaterial(handle)) {
        LUNA_CORE_WARN("Ignoring material override for non-built-in material handle {}", handle.toString());
        return false;
    }

    auto material = AssetManager::get().loadAssetAs<Material>(handle);
    if (!material) {
        LUNA_CORE_WARN("Failed to apply built-in material override because material {} is not loaded", handle.toString());
        return false;
    }

    Material::SurfaceProperties surface = material->getSurface();
    surface.BaseColorFactor = readVec4(node["BaseColorFactor"], surface.BaseColorFactor);
    surface.EmissiveFactor = readVec3(node["EmissiveFactor"], surface.EmissiveFactor);
    if (node["MetallicFactor"]) {
        surface.MetallicFactor = node["MetallicFactor"].as<float>();
    }
    if (node["RoughnessFactor"]) {
        surface.RoughnessFactor = node["RoughnessFactor"].as<float>();
    }
    if (node["AlphaCutoff"]) {
        surface.AlphaCutoff = node["AlphaCutoff"].as<float>();
    }
    if (node["Unlit"]) {
        surface.Unlit = node["Unlit"].as<bool>();
    }

    material->setSurface(surface);
    return true;
}

void emitMaterialOverride(YAML::Emitter& out, AssetHandle handle, const Material& material)
{
    const auto& surface = material.getSurface();
    out << YAML::Key << handle.toString() << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "BaseColorFactor" << YAML::Value;
    emitVec4(out, surface.BaseColorFactor);
    out << YAML::Key << "EmissiveFactor" << YAML::Value;
    emitVec3(out, surface.EmissiveFactor);
    out << YAML::Key << "MetallicFactor" << YAML::Value << surface.MetallicFactor;
    out << YAML::Key << "RoughnessFactor" << YAML::Value << surface.RoughnessFactor;
    out << YAML::Key << "AlphaCutoff" << YAML::Value << surface.AlphaCutoff;
    out << YAML::Key << "Unlit" << YAML::Value << surface.Unlit;
    out << YAML::EndMap;
}

bool writeCurrentOverridesToPath(const std::filesystem::path& overrides_path)
{
    std::error_code ec;
    std::filesystem::create_directories(overrides_path.parent_path(), ec);
    if (ec) {
        LUNA_CORE_ERROR("Failed to create project settings directory '{}': {}",
                        overrides_path.parent_path().string(),
                        ec.message());
        return false;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << kRootKey << YAML::Value << YAML::BeginMap;
    for (const auto& descriptor : BuiltinAssets::getBuiltinMaterials()) {
        auto material = AssetManager::get().loadAssetAs<Material>(descriptor.Handle);
        if (!material) {
            continue;
        }
        emitMaterialOverride(out, descriptor.Handle, *material);
    }
    out << YAML::EndMap;
    out << YAML::EndMap;

    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to emit built-in material overrides YAML: {}", out.GetLastError());
        return false;
    }

    std::ofstream output_stream(overrides_path);
    if (!output_stream.is_open()) {
        LUNA_CORE_ERROR("Failed to open built-in material overrides file for writing: {}", overrides_path.string());
        return false;
    }
    output_stream << out.c_str();
    LUNA_CORE_INFO("Saved built-in material overrides to '{}'", overrides_path.string());
    return output_stream.good();
}

} // namespace

std::filesystem::path BuiltinMaterialOverrides::getOverridesPath()
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    if (!project_root) {
        return {};
    }
    return (*project_root / kSettingsDirectory / kOverridesFile).lexically_normal();
}

bool BuiltinMaterialOverrides::load()
{
    const std::filesystem::path overrides_path = getOverridesPath();
    if (overrides_path.empty()) {
        LUNA_CORE_WARN("Cannot load built-in material overrides because no project is loaded");
        return false;
    }

    if (!std::filesystem::exists(overrides_path)) {
        LUNA_CORE_INFO("No built-in material overrides found at '{}'", overrides_path.string());
        return true;
    }

    YAML::Node root;
    try {
        root = YAML::LoadFile(overrides_path.string());
    } catch (const YAML::Exception& error) {
        LUNA_CORE_ERROR("Failed to load built-in material overrides '{}': {}", overrides_path.string(), error.what());
        return false;
    }

    const YAML::Node overrides = root[kRootKey];
    if (!overrides || !overrides.IsMap()) {
        LUNA_CORE_WARN("Built-in material overrides file has no '{}' map: {}", kRootKey, overrides_path.string());
        return false;
    }

    size_t applied_count = 0;
    for (const auto& entry : overrides) {
        const AssetHandle handle(entry.first.as<uint64_t>());
        if (applyOverride(handle, entry.second)) {
            ++applied_count;
        }
    }

    LUNA_CORE_INFO("Loaded {} built-in material override(s) from '{}'", applied_count, overrides_path.string());
    return true;
}

bool BuiltinMaterialOverrides::save()
{
    const std::filesystem::path overrides_path = getOverridesPath();
    if (overrides_path.empty()) {
        LUNA_CORE_WARN("Cannot save built-in material overrides because no project is loaded");
        return false;
    }
    return writeCurrentOverridesToPath(overrides_path);
}

bool BuiltinMaterialOverrides::clearSelected(AssetHandle material_handle)
{
    if (!BuiltinAssets::isBuiltinMaterial(material_handle)) {
        return false;
    }

    if (auto material = AssetManager::get().loadAssetAs<Material>(material_handle)) {
        material->resetSurface();
        return save();
    }

    return false;
}

bool BuiltinMaterialOverrides::clearAll()
{
    for (const auto& descriptor : BuiltinAssets::getBuiltinMaterials()) {
        if (auto material = AssetManager::get().loadAssetAs<Material>(descriptor.Handle)) {
            material->resetSurface();
        }
    }

    const std::filesystem::path overrides_path = getOverridesPath();
    if (!overrides_path.empty() && std::filesystem::exists(overrides_path)) {
        std::error_code ec;
        std::filesystem::remove(overrides_path, ec);
        if (ec) {
            LUNA_CORE_ERROR("Failed to remove built-in material overrides file '{}': {}",
                            overrides_path.string(),
                            ec.message());
            return false;
        }
        LUNA_CORE_INFO("Removed built-in material overrides file '{}'", overrides_path.string());
    }

    return true;
}

} // namespace luna
