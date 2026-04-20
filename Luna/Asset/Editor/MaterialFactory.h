#pragma once

#include "Asset/Asset.h"
#include "Renderer/Material.h"

#include <filesystem>
#include <string>

namespace luna {

struct MaterialTextureHandleSet {
    AssetHandle BaseColor{0};
    AssetHandle Normal{0};
    AssetHandle MetallicRoughness{0};
    AssetHandle Emissive{0};
    AssetHandle Occlusion{0};
};

struct MaterialAssetDescriptor {
    std::string Name;
    MaterialTextureHandleSet Textures;
    Material::SurfaceProperties Surface;
};

class MaterialFactory final {
public:
    static MaterialAssetDescriptor makeDefaultDescriptor(std::string name = "DefaultMaterial");

    static bool createMaterialFile(const std::filesystem::path& path,
                                   const MaterialAssetDescriptor& descriptor,
                                   bool create_metadata = true);

    static bool createDefaultMaterialFile(const std::filesystem::path& path,
                                          std::string name = "DefaultMaterial",
                                          bool create_metadata = true);
};

} // namespace luna
