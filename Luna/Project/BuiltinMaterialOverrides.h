#pragma once

#include "Asset/Asset.h"

#include <filesystem>

namespace luna {

class BuiltinMaterialOverrides final {
public:
    static std::filesystem::path getOverridesPath();
    static bool load();
    static bool save();
    static bool clearSelected(AssetHandle material_handle);
    static bool clearAll();
};

} // namespace luna
