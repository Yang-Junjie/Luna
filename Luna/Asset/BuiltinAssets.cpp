#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/PrimitiveMeshFactory.h"

#include <glm/vec4.hpp>
#include <utility>

namespace luna {
namespace {

const std::array<BuiltinMeshDescriptor, 5> kBuiltinMeshes{{
    {BuiltinMeshes::Cube, "Cube"},
    {BuiltinMeshes::Sphere, "Sphere"},
    {BuiltinMeshes::Plane, "Plane"},
    {BuiltinMeshes::Cylinder, "Cylinder"},
    {BuiltinMeshes::Cone, "Cone"},
}};

const std::array<BuiltinMaterialDescriptor, 6> kBuiltinMaterials{{
    {BuiltinMaterials::DefaultLit, "Default Lit"},
    {BuiltinMaterials::DefaultUnlit, "Default Unlit"},
    {BuiltinMaterials::DebugRed, "Debug Red"},
    {BuiltinMaterials::DebugGreen, "Debug Green"},
    {BuiltinMaterials::DebugBlue, "Debug Blue"},
    {BuiltinMaterials::Transparent, "Transparent"},
}};

void setBuiltinMetadata(AssetHandle handle, AssetType type, const char* name, const char* path)
{
    AssetMetadata metadata{};
    metadata.Handle = handle;
    metadata.Type = type;
    metadata.Name = name;
    metadata.FilePath = path;
    metadata.MemoryOnly = true;
    AssetDatabase::set(handle, metadata);
}

std::shared_ptr<Material> createBuiltinMaterial(const char* name,
                                                const glm::vec4& base_color,
                                                bool unlit = false,
                                                Material::BlendMode blend_mode = Material::BlendMode::Opaque)
{
    Material::TextureSet textures{};
    Material::SurfaceProperties surface{};
    surface.BaseColorFactor = base_color;
    surface.MetallicFactor = 0.0f;
    surface.RoughnessFactor = 1.0f;
    surface.BlendModeValue = blend_mode;
    surface.Unlit = unlit;
    return Material::create(name, std::move(textures), surface);
}

} // namespace

void BuiltinAssets::registerAll()
{
    auto& asset_manager = AssetManager::get();

    if (asset_manager.isAssetLoaded(BuiltinMeshes::Cube) && asset_manager.isAssetLoaded(BuiltinMeshes::Sphere) &&
        asset_manager.isAssetLoaded(BuiltinMeshes::Plane) && asset_manager.isAssetLoaded(BuiltinMeshes::Cylinder) &&
        asset_manager.isAssetLoaded(BuiltinMeshes::Cone) && asset_manager.isAssetLoaded(BuiltinMaterials::DefaultLit) &&
        asset_manager.isAssetLoaded(BuiltinMaterials::DefaultUnlit) &&
        asset_manager.isAssetLoaded(BuiltinMaterials::DebugRed) &&
        asset_manager.isAssetLoaded(BuiltinMaterials::DebugGreen) &&
        asset_manager.isAssetLoaded(BuiltinMaterials::DebugBlue) &&
        asset_manager.isAssetLoaded(BuiltinMaterials::Transparent) && AssetDatabase::exists(BuiltinMeshes::Cube) &&
        AssetDatabase::exists(BuiltinMeshes::Sphere) && AssetDatabase::exists(BuiltinMeshes::Plane) &&
        AssetDatabase::exists(BuiltinMeshes::Cylinder) && AssetDatabase::exists(BuiltinMeshes::Cone) &&
        AssetDatabase::exists(BuiltinMaterials::DefaultLit) && AssetDatabase::exists(BuiltinMaterials::DefaultUnlit) &&
        AssetDatabase::exists(BuiltinMaterials::DebugRed) && AssetDatabase::exists(BuiltinMaterials::DebugGreen) &&
        AssetDatabase::exists(BuiltinMaterials::DebugBlue) && AssetDatabase::exists(BuiltinMaterials::Transparent)) {
        LUNA_CORE_TRACE("Built-in assets already registered");
        return;
    }

    LUNA_CORE_DEBUG("Registering built-in assets");

    asset_manager.registerMemoryAsset(BuiltinMeshes::Cube, PrimitiveMeshFactory::createCube());
    setBuiltinMetadata(BuiltinMeshes::Cube, AssetType::Mesh, "Cube", "builtin://mesh/cube");
    LUNA_CORE_TRACE("Registered built-in mesh '{}' handle={}", "Cube", BuiltinMeshes::Cube.toString());

    asset_manager.registerMemoryAsset(BuiltinMeshes::Sphere, PrimitiveMeshFactory::createSphere());
    setBuiltinMetadata(BuiltinMeshes::Sphere, AssetType::Mesh, "Sphere", "builtin://mesh/sphere");
    LUNA_CORE_TRACE("Registered built-in mesh '{}' handle={}", "Sphere", BuiltinMeshes::Sphere.toString());

    asset_manager.registerMemoryAsset(BuiltinMeshes::Plane, PrimitiveMeshFactory::createPlane());
    setBuiltinMetadata(BuiltinMeshes::Plane, AssetType::Mesh, "Plane", "builtin://mesh/plane");
    LUNA_CORE_TRACE("Registered built-in mesh '{}' handle={}", "Plane", BuiltinMeshes::Plane.toString());

    asset_manager.registerMemoryAsset(BuiltinMeshes::Cylinder, PrimitiveMeshFactory::createCylinder());
    setBuiltinMetadata(BuiltinMeshes::Cylinder, AssetType::Mesh, "Cylinder", "builtin://mesh/cylinder");
    LUNA_CORE_TRACE("Registered built-in mesh '{}' handle={}", "Cylinder", BuiltinMeshes::Cylinder.toString());

    asset_manager.registerMemoryAsset(BuiltinMeshes::Cone, PrimitiveMeshFactory::createCone());
    setBuiltinMetadata(BuiltinMeshes::Cone, AssetType::Mesh, "Cone", "builtin://mesh/cone");
    LUNA_CORE_TRACE("Registered built-in mesh '{}' handle={}", "Cone", BuiltinMeshes::Cone.toString());

    asset_manager.registerMemoryAsset(BuiltinMaterials::DefaultLit,
                                      createBuiltinMaterial("Default Lit", {1.0f, 1.0f, 1.0f, 1.0f}));
    setBuiltinMetadata(
        BuiltinMaterials::DefaultLit, AssetType::Material, "Default Lit", "builtin://material/default-lit");
    LUNA_CORE_TRACE(
        "Registered built-in material '{}' handle={}", "Default Lit", BuiltinMaterials::DefaultLit.toString());

    asset_manager.registerMemoryAsset(BuiltinMaterials::DefaultUnlit,
                                      createBuiltinMaterial("Default Unlit", {1.0f, 1.0f, 1.0f, 1.0f}, true));
    setBuiltinMetadata(
        BuiltinMaterials::DefaultUnlit, AssetType::Material, "Default Unlit", "builtin://material/default-unlit");
    LUNA_CORE_TRACE(
        "Registered built-in material '{}' handle={}", "Default Unlit", BuiltinMaterials::DefaultUnlit.toString());

    asset_manager.registerMemoryAsset(BuiltinMaterials::DebugRed,
                                      createBuiltinMaterial("Debug Red", {1.0f, 0.1f, 0.1f, 1.0f}, true));
    setBuiltinMetadata(BuiltinMaterials::DebugRed, AssetType::Material, "Debug Red", "builtin://material/debug-red");
    LUNA_CORE_TRACE("Registered built-in material '{}' handle={}", "Debug Red", BuiltinMaterials::DebugRed.toString());

    asset_manager.registerMemoryAsset(BuiltinMaterials::DebugGreen,
                                      createBuiltinMaterial("Debug Green", {0.1f, 1.0f, 0.1f, 1.0f}, true));
    setBuiltinMetadata(
        BuiltinMaterials::DebugGreen, AssetType::Material, "Debug Green", "builtin://material/debug-green");
    LUNA_CORE_TRACE(
        "Registered built-in material '{}' handle={}", "Debug Green", BuiltinMaterials::DebugGreen.toString());

    asset_manager.registerMemoryAsset(BuiltinMaterials::DebugBlue,
                                      createBuiltinMaterial("Debug Blue", {0.1f, 0.25f, 1.0f, 1.0f}, true));
    setBuiltinMetadata(BuiltinMaterials::DebugBlue, AssetType::Material, "Debug Blue", "builtin://material/debug-blue");
    LUNA_CORE_TRACE(
        "Registered built-in material '{}' handle={}", "Debug Blue", BuiltinMaterials::DebugBlue.toString());

    asset_manager.registerMemoryAsset(
        BuiltinMaterials::Transparent,
        createBuiltinMaterial("Transparent", {1.0f, 1.0f, 1.0f, 0.35f}, false, Material::BlendMode::Transparent));
    setBuiltinMetadata(
        BuiltinMaterials::Transparent, AssetType::Material, "Transparent", "builtin://material/transparent");
    LUNA_CORE_TRACE(
        "Registered built-in material '{}' handle={}", "Transparent", BuiltinMaterials::Transparent.toString());
}

bool BuiltinAssets::isBuiltinAsset(AssetHandle handle)
{
    return isBuiltinMesh(handle) || isBuiltinMaterial(handle);
}

bool BuiltinAssets::isBuiltinMesh(AssetHandle handle)
{
    for (const auto& mesh : kBuiltinMeshes) {
        if (mesh.Handle == handle) {
            return true;
        }
    }
    return false;
}

bool BuiltinAssets::isBuiltinMaterial(AssetHandle handle)
{
    for (const auto& material : kBuiltinMaterials) {
        if (material.Handle == handle) {
            return true;
        }
    }
    return false;
}

const char* BuiltinAssets::getDisplayName(AssetHandle handle)
{
    for (const auto& mesh : kBuiltinMeshes) {
        if (mesh.Handle == handle) {
            return mesh.Name;
        }
    }

    for (const auto& material : kBuiltinMaterials) {
        if (material.Handle == handle) {
            return material.Name;
        }
    }

    return "";
}

const std::array<BuiltinMeshDescriptor, 5>& BuiltinAssets::getBuiltinMeshes()
{
    return kBuiltinMeshes;
}

const std::array<BuiltinMaterialDescriptor, 6>& BuiltinAssets::getBuiltinMaterials()
{
    return kBuiltinMaterials;
}

} // namespace luna
