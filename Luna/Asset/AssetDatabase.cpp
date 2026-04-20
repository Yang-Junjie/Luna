#include "AssetDatabase.h"

#include "Core/FileTool.h"
#include "Project/ProjectManager.h"

namespace luna {
AssetMetadata& AssetDatabase::getAssetMetadata(AssetHandle handle)
{
    return s_database.at(handle);
}

void AssetDatabase::set(AssetHandle handle, const AssetMetadata& info)
{
    s_database[handle] = info;
}

bool AssetDatabase::exists(AssetHandle handle)
{
    return s_database.contains(handle);
}

size_t AssetDatabase::remove(AssetHandle handle)
{
    return s_database.erase(handle);
}

void AssetDatabase::clear()
{
    s_database.clear();
}

AssetHandle AssetDatabase::findHandleByFilePath(const std::filesystem::path& file_path)
{
    if (file_path.empty()) {
        return AssetHandle(0);
    }

    std::filesystem::path normalized_path = file_path.lexically_normal();
    if (normalized_path.is_absolute()) {
        if (const auto project_root = ProjectManager::instance().getProjectRootPath()) {
            normalized_path = tools::makeRelative(normalized_path, *project_root);
        }
    }

    const std::string target = normalized_path.lexically_normal().generic_string();
    for (const auto& [handle, metadata] : s_database) {
        if (metadata.FilePath.lexically_normal().generic_string() == target) {
            return handle;
        }
    }

    return AssetHandle(0);
}

const std::unordered_map<AssetHandle, AssetMetadata>& AssetDatabase::getDatabase()
{
    return s_database;
}
} // namespace luna
