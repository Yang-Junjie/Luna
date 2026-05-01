#pragma once

#include "Asset/Asset.h"

#include <string>

namespace luna {

class ScriptAsset final : public Asset {
public:
    std::string source;
    std::string language;

    AssetType getAssetsType() const override
    {
        return AssetType::Script;
    }
};

} // namespace luna
