#pragma once

#include "Asset/AssetMetadata.h"

#include <cstddef>

#include <filesystem>

namespace luna {

class FbxModelAssetGenerator final {
public:
    struct GenerateResult {
        bool Success = false;
        bool CreatedModelFile = false;
        bool CreatedModelMetadata = false;
        size_t CreatedMaterialFiles = 0;
        size_t CreatedMaterialMetadata = 0;
        size_t CreatedTextureMetadata = 0;
    };

    static GenerateResult generateCompanionAssets(const std::filesystem::path& fbx_path,
                                                  const AssetMetadata& mesh_metadata);
};

} // namespace luna
