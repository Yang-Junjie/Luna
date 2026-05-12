#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace luna::editor {

struct AssetInfo {
    AssetHandle handle{0};
    AssetType type{AssetType::None};
    std::string label;
    std::string detail;
    bool exists{false};
    bool builtin{false};
    bool loading{false};
};

class AssetService {
public:
    virtual ~AssetService() = default;

    virtual AssetInfo describeAsset(AssetHandle handle) const = 0;
    virtual std::vector<AssetInfo> builtinAssets(AssetType type) const = 0;
    virtual bool isAssetLoading(AssetHandle handle) const = 0;
    virtual bool acceptsAssetType(AssetType type, const AssetType* accepted_types, std::size_t accepted_type_count) const = 0;
    virtual std::optional<std::size_t> meshSubmeshCount(AssetHandle mesh_handle) const = 0;

    bool acceptsAssetType(AssetType type, std::initializer_list<AssetType> accepted_types) const
    {
        return acceptsAssetType(type, accepted_types.begin(), accepted_types.size());
    }
};

} // namespace luna::editor
