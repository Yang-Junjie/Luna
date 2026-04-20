#pragma once

#include "Asset.h"
#include "AssetDatabase.h"

#include <memory>
#include <unordered_map>

namespace luna {

class AssetManager {
public:
    ~AssetManager();

    static AssetManager& get();

    void init();
    void clear();
    std::shared_ptr<Asset> loadAsset(AssetHandle handle);
    void registerMemoryAsset(AssetHandle handle, const std::shared_ptr<Asset>& asset);

    template <typename T> std::shared_ptr<T> loadAssetAs(AssetHandle handle)
    {
        return std::dynamic_pointer_cast<T>(loadAsset(handle));
    }

private:
    AssetManager() = default;

    std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_loaded_assets;
};
} // namespace luna
