#pragma once
#include "Asset.h"

#include <filesystem>

namespace luna {
struct AssetMetadata {
    AssetHandle Handle;
    AssetType Type;
    std::filesystem::path FilePath;
    std::string Name;

    bool MemoryOnly = false;
};
} // namespace luna
