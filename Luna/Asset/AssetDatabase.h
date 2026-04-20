#pragma once
#include "AssetMetadata.h"

#include <filesystem>
#include <unordered_map>

namespace luna {
class AssetDatabase {
public:
    static AssetMetadata& getAssetMetadata(AssetHandle handle);
    static void set(AssetHandle handle, const AssetMetadata& info);
    static bool exists(AssetHandle handle);
    static size_t remove(AssetHandle handle);
    static void clear();
    static AssetHandle findHandleByFilePath(const std::filesystem::path& file_path);
    static const std::unordered_map<AssetHandle, AssetMetadata>& getDatabase();

private:
    inline static std::unordered_map<AssetHandle, AssetMetadata> s_database;
};
} // namespace luna
