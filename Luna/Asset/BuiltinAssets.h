#pragma once

#include "Asset/Asset.h"

#include <array>
#include <cstddef>

namespace luna {

namespace BuiltinMeshes {
inline const AssetHandle Cube{1001};
inline const AssetHandle Sphere{1002};
inline const AssetHandle Plane{1003};
inline const AssetHandle Cylinder{1004};
inline const AssetHandle Cone{1005};
} // namespace BuiltinMeshes

namespace BuiltinMaterials {
inline const AssetHandle DefaultLit{2001};
} // namespace BuiltinMaterials

struct BuiltinMeshDescriptor {
    AssetHandle Handle;
    const char* Name;
};

class BuiltinAssets final {
public:
    static void registerAll();
    static bool isBuiltinAsset(AssetHandle handle);
    static bool isBuiltinMesh(AssetHandle handle);
    static bool isBuiltinMaterial(AssetHandle handle);
    static const char* getDisplayName(AssetHandle handle);
    static const std::array<BuiltinMeshDescriptor, 5>& getBuiltinMeshes();
};

} // namespace luna
