#include "Asset/BuiltinAssets.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/PrimitiveMeshFactory.h"

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

} // namespace

void BuiltinAssets::registerAll()
{
    auto& asset_manager = AssetManager::get();

    if (asset_manager.isAssetLoaded(BuiltinMeshes::Cube) && asset_manager.isAssetLoaded(BuiltinMeshes::Sphere) &&
        asset_manager.isAssetLoaded(BuiltinMeshes::Plane) && asset_manager.isAssetLoaded(BuiltinMeshes::Cylinder) &&
        asset_manager.isAssetLoaded(BuiltinMeshes::Cone) && asset_manager.isAssetLoaded(BuiltinMaterials::DefaultLit) &&
        AssetDatabase::exists(BuiltinMeshes::Cube) && AssetDatabase::exists(BuiltinMeshes::Sphere) &&
        AssetDatabase::exists(BuiltinMeshes::Plane) && AssetDatabase::exists(BuiltinMeshes::Cylinder) &&
        AssetDatabase::exists(BuiltinMeshes::Cone) && AssetDatabase::exists(BuiltinMaterials::DefaultLit)) {
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

    Material::TextureSet textures{};
    Material::SurfaceProperties surface{};
    surface.BaseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    surface.MetallicFactor = 0.0f;
    surface.RoughnessFactor = 1.0f;
    asset_manager.registerMemoryAsset(BuiltinMaterials::DefaultLit,
                                      Material::create("Default Lit", std::move(textures), surface));
    setBuiltinMetadata(BuiltinMaterials::DefaultLit, AssetType::Material, "Default Lit", "builtin://material/default-lit");
    LUNA_CORE_TRACE("Registered built-in material '{}' handle={}", "Default Lit", BuiltinMaterials::DefaultLit.toString());
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
    return handle == BuiltinMaterials::DefaultLit;
}

const char* BuiltinAssets::getDisplayName(AssetHandle handle)
{
    for (const auto& mesh : kBuiltinMeshes) {
        if (mesh.Handle == handle) {
            return mesh.Name;
        }
    }

    if (handle == BuiltinMaterials::DefaultLit) {
        return "Default Lit";
    }

    return "";
}

const std::array<BuiltinMeshDescriptor, 5>& BuiltinAssets::getBuiltinMeshes()
{
    return kBuiltinMeshes;
}

} // namespace luna
