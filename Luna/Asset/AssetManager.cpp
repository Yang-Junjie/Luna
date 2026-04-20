#include "AssetManager.h"

#include "Asset/Editor/LoaderManager.h"

namespace luna {

AssetManager::~AssetManager() = default;

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
    m_loaded_assets.clear();
}

void AssetManager::registerMemoryAsset(AssetHandle handle, const std::shared_ptr<Asset>& asset)
{
    if (!asset || !handle.isValid()) {
        return;
    }

    asset->handle = handle;
    m_loaded_assets[handle] = asset;

    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = asset->getAssetsType();
    metadata.MemoryOnly = true;
    AssetDatabase::set(handle, metadata);
}

std::shared_ptr<Asset> AssetManager::loadAsset(AssetHandle handle)
{
    if (const auto cached = m_loaded_assets.find(handle); cached != m_loaded_assets.end()) {
        return cached->second;
    }

    if (!AssetDatabase::exists(handle)) {
        return {};
    }

    init();

    const AssetMetadata& metadata = AssetDatabase::getAssetMetadata(handle);
    Loader* loader = LoaderManager::getLoader(metadata.Type);
    if (loader == nullptr) {
        return {};
    }

    std::shared_ptr<Asset> asset = loader->load(metadata);
    if (!asset) {
        return {};
    }

    asset->handle = handle;
    m_loaded_assets[handle] = asset;
    return asset;
}

} // namespace luna
