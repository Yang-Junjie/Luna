#include "AssetManager.h"

#include "Asset/Editor/LoaderManager.h"
#include "Core/Log.h"

#include <vector>

namespace {

constexpr uint32_t kAssetLoadWorkerThreads = 1;

}

namespace luna {

AssetManager::~AssetManager()
{
    clear();
}

AssetManager& AssetManager::get()
{
    static AssetManager asset_manager;
    return asset_manager;
}

void AssetManager::init()
{
    LoaderManager::init();
}

void AssetManager::clear()
{
    bool has_pending_loads = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        has_pending_loads = m_background_task_system_initialized && !m_pending_asset_loads.empty();
    }

    if (has_pending_loads) {
        // Finish background asset work before clearing caches so worker threads cannot write stale results later.
        m_background_task_system.waitForAll();
    }

    if (m_background_task_system_initialized) {
        m_background_task_system.pollCompletedTasks();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_loaded_assets.clear();
    m_failed_assets.clear();
    m_pending_asset_loads.clear();
}

void AssetManager::updateAsyncLoads()
{
    if (!m_background_task_system_initialized) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pending_asset_loads.empty()) {
            return;
        }
    }

    finalizeCompletedLoads();
}

void AssetManager::ensureBackgroundTaskSystem()
{
    if (m_background_task_system_initialized) {
        return;
    }

    TaskSystemConfig config{};
    config.worker_thread_count = kAssetLoadWorkerThreads;
    config.io_thread_count = 0;
    config.external_thread_count = 0;

    if (!m_background_task_system.initialize(config)) {
        LUNA_CORE_WARN("AssetManager background task system initialization failed; async asset loading disabled");
        return;
    }

    m_background_task_system_initialized = true;
}

void AssetManager::finalizeCompletedLoads()
{
    if (m_background_task_system_initialized) {
        m_background_task_system.pollCompletedTasks();
    }

    std::vector<std::pair<AssetHandle, std::shared_ptr<Asset>>> loaded_results;
    std::vector<AssetHandle> failed_handles;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_pending_asset_loads.begin(); it != m_pending_asset_loads.end();) {
            const AssetHandle handle = it->first;
            const std::shared_ptr<PendingAssetLoad>& pending = it->second;
            if (!pending || !pending->task.isValid() || !pending->task.isComplete()) {
                ++it;
                continue;
            }

            std::shared_ptr<Asset> asset;
            bool failed = false;
            {
                std::lock_guard<std::mutex> pending_lock(pending->mutex);
                asset = pending->asset;
                failed = pending->failed || !asset;
            }

            if (asset) {
                m_loaded_assets[handle] = asset;
                loaded_results.emplace_back(handle, asset);
            } else if (failed) {
                m_failed_assets.insert(handle);
                failed_handles.push_back(handle);
            }

            it = m_pending_asset_loads.erase(it);
        }
    }

    for (const auto& [handle, asset] : loaded_results) {
        (void) handle;
        (void) asset;
    }

    for (const AssetHandle handle : failed_handles) {
        LUNA_CORE_WARN("Async asset load failed for handle {}", handle.toString());
    }
}

void AssetManager::registerMemoryAsset(AssetHandle handle, const std::shared_ptr<Asset>& asset)
{
    if (!asset || !handle.isValid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    asset->handle = handle;
    m_loaded_assets[handle] = asset;
    m_failed_assets.erase(handle);
    m_pending_asset_loads.erase(handle);

    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = asset->getAssetsType();
    metadata.MemoryOnly = true;
    AssetDatabase::set(handle, metadata);
}

std::shared_ptr<Asset> AssetManager::loadAsset(AssetHandle handle)
{
    return loadAssetInternal(handle, false);
}

std::shared_ptr<Asset> AssetManager::requestAsset(AssetHandle handle)
{
    return loadAssetInternal(handle, true);
}

bool AssetManager::isAssetLoading(AssetHandle handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pending_asset_loads.contains(handle);
}

bool AssetManager::isAssetLoaded(AssetHandle handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loaded_assets.contains(handle);
}

std::vector<AssetManager::LoadingAssetInfo> AssetManager::getLoadingAssetsSnapshot()
{
    std::vector<LoadingAssetInfo> loading_assets;

    std::lock_guard<std::mutex> lock(m_mutex);
    loading_assets.reserve(m_pending_asset_loads.size());

    for (const auto& [handle, pending] : m_pending_asset_loads) {
        (void) pending;

        LoadingAssetInfo info;
        info.Handle = handle;

        if (AssetDatabase::exists(handle)) {
            const AssetMetadata& metadata = AssetDatabase::getAssetMetadata(handle);
            info.Type = metadata.Type;
            info.Name = metadata.Name;
            info.FilePath = metadata.FilePath;
        }

        loading_assets.push_back(std::move(info));
    }

    return loading_assets;
}

std::shared_ptr<Asset> AssetManager::loadAssetInternal(AssetHandle handle, bool allow_async_request)
{
    if (!handle.isValid()) {
        return {};
    }

    std::shared_ptr<PendingAssetLoad> pending_load;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (const auto cached = m_loaded_assets.find(handle); cached != m_loaded_assets.end()) {
            return cached->second;
        }

        if (m_failed_assets.contains(handle)) {
            return {};
        }

        if (const auto pending_it = m_pending_asset_loads.find(handle); pending_it != m_pending_asset_loads.end()) {
            if (allow_async_request) {
                return {};
            }

            pending_load = pending_it->second;
        }
    }

    if (!allow_async_request) {
        finalizeCompletedLoads();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (const auto cached = m_loaded_assets.find(handle); cached != m_loaded_assets.end()) {
                return cached->second;
            }

            if (m_failed_assets.contains(handle)) {
                return {};
            }
        }
    }

    if (pending_load) {
        if (pending_load->task.isValid() && !pending_load->task.isComplete()) {
            pending_load->task.wait(m_background_task_system);
        }

        finalizeCompletedLoads();

        std::lock_guard<std::mutex> lock(m_mutex);
        if (const auto cached = m_loaded_assets.find(handle); cached != m_loaded_assets.end()) {
            return cached->second;
        }

        return {};
    }

    if (!AssetDatabase::exists(handle)) {
        return {};
    }

    init();

    const AssetMetadata metadata = AssetDatabase::getAssetMetadata(handle);
    Loader* loader = LoaderManager::getLoader(metadata.Type);
    if (loader == nullptr) {
        return {};
    }

    if (!allow_async_request) {
        std::shared_ptr<Asset> asset = loader->load(metadata);
        if (!asset) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_failed_assets.insert(handle);
            return {};
        }

        asset->handle = handle;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_loaded_assets[handle] = asset;
        m_failed_assets.erase(handle);
        m_pending_asset_loads.erase(handle);
        return asset;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (const auto cached = m_loaded_assets.find(handle); cached != m_loaded_assets.end()) {
            return cached->second;
        }

        if (m_failed_assets.contains(handle) || m_pending_asset_loads.contains(handle)) {
            return {};
        }
    }

    ensureBackgroundTaskSystem();
    if (!m_background_task_system_initialized) {
        return loadAssetInternal(handle, false);
    }

    auto pending = std::make_shared<PendingAssetLoad>();
    TaskSubmitDesc submit_desc{};
    submit_desc.priority = enki::TASK_PRIORITY_LOW;
    TaskHandle task = m_background_task_system.submit(
        [pending, loader, metadata, handle]() {
            std::shared_ptr<Asset> asset = loader->load(metadata);
            if (asset) {
                asset->handle = handle;
            }

            std::lock_guard<std::mutex> lock(pending->mutex);
            pending->asset = std::move(asset);
            pending->failed = pending->asset == nullptr;
        },
        submit_desc);

    if (!task.isValid()) {
        return loadAssetInternal(handle, false);
    }

    pending->task = task;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending_asset_loads[handle] = std::move(pending);
    return {};
}

} // namespace luna
