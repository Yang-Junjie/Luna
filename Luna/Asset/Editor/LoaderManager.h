#pragma once

#include "Asset/AssetTypes.h"
#include "Loader.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace luna {

class LoaderManager final {
public:
    static void init();
    static Loader* getLoader(AssetType type);

private:
    static inline std::vector<std::unique_ptr<Loader>> loaders;
    static inline std::unordered_map<AssetType, Loader*> type_to_loader;
};

} // namespace luna
