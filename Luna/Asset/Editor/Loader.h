#pragma once
#include "../AssetMetadata.h"

namespace luna {
class Loader {
public:
    virtual ~Loader() = default;
    virtual std::shared_ptr<Asset> load(const AssetMetadata& meta_data) = 0;
};
} // namespace luna
