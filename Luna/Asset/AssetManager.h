#pragma once

#include "Asset.h"
#include "AssetDatabase.h"
#include "JobSystem/TaskHandle.h"
#include "JobSystem/TaskSystem.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace luna {

class AssetManager {
public:
    struct LoadingAssetInfo {
        AssetHandle Handle = AssetHandle(0);
        AssetType Type = AssetType::None;
        std::string Name;
        std::filesystem::path FilePath;
    };

    ~AssetManager();

    static AssetManager& get();

    void init();
    void clear();
    void updateAsyncLoads();
    std::shared_ptr<Asset> loadAsset(AssetHandle handle);
    std::shared_ptr<Asset> requestAsset(AssetHandle handle);
    void registerMemoryAsset(AssetHandle handle, const std::shared_ptr<Asset>& asset);
    bool isAssetLoading(AssetHandle handle);
    bool isAssetLoaded(AssetHandle handle);
    std::vector<LoadingAssetInfo> getLoadingAssetsSnapshot();

    template <typename T> std::shared_ptr<T> loadAssetAs(AssetHandle handle)
    {
        return std::dynamic_pointer_cast<T>(loadAsset(handle));
    }

    template <typename T> std::shared_ptr<T> requestAssetAs(AssetHandle handle)
    {
        return std::dynamic_pointer_cast<T>(requestAsset(handle));
    }

private:
    struct PendingAssetLoad {
        TaskHandle task;
        std::mutex mutex;
        std::shared_ptr<Asset> asset;
        bool failed = false;
    };

    AssetManager() = default;
    void ensureBackgroundTaskSystem();
    std::shared_ptr<Asset> loadAssetInternal(AssetHandle handle, bool allow_async_request);
    void finalizeCompletedLoads();

    std::mutex m_mutex;
    std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_loaded_assets;
    std::unordered_map<AssetHandle, std::shared_ptr<PendingAssetLoad>> m_pending_asset_loads;
    std::unordered_set<AssetHandle> m_failed_assets;
    TaskSystem m_background_task_system;
    bool m_background_task_system_initialized = false;
};
} // namespace luna
