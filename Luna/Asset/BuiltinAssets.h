#pragma once

#include "Asset/Asset.h"

#include <cstddef>

#include <array>

namespace luna {

namespace BuiltinMeshes {
inline const AssetHandle Cube{1'001};
inline const AssetHandle Sphere{1'002};
inline const AssetHandle Plane{1'003};
inline const AssetHandle Cylinder{1'004};
inline const AssetHandle Cone{1'005};
} // namespace BuiltinMeshes

namespace BuiltinMaterials {
inline const AssetHandle DefaultLit{2'001};
inline const AssetHandle DefaultUnlit{2'002};
inline const AssetHandle DebugRed{2'003};
inline const AssetHandle DebugGreen{2'004};
inline const AssetHandle DebugBlue{2'005};
inline const AssetHandle Transparent{2'006};
} // namespace BuiltinMaterials

struct BuiltinMeshDescriptor {
    AssetHandle Handle{0};
    const char* Name = "";
};

struct BuiltinMaterialDescriptor {
    AssetHandle Handle{0};
    const char* Name = "";
};

class BuiltinAssets final {
public:
    static void registerAll();
    static bool isBuiltinAsset(AssetHandle handle);
    static bool isBuiltinMesh(AssetHandle handle);
    static bool isBuiltinMaterial(AssetHandle handle);
    static const char* getDisplayName(AssetHandle handle);
    static const std::array<BuiltinMeshDescriptor, 5>& getBuiltinMeshes();
    static const std::array<BuiltinMaterialDescriptor, 6>& getBuiltinMaterials();
};

} // namespace luna
