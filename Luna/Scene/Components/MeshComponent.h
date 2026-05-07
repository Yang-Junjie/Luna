#pragma once

#include "Asset/Asset.h"

#include <cstddef>
#include <cstdint>

#include <vector>

namespace luna {

struct MeshComponent {
    AssetHandle meshHandle = AssetHandle(0);

    std::vector<AssetHandle> submeshMaterials;

    MeshComponent() = default;

    MeshComponent(const MeshComponent&) = default;

    void setSubmeshMaterial(uint32_t submeshIndex, AssetHandle materialHandle)
    {
        if (submeshIndex >= submeshMaterials.size()) {
            submeshMaterials.resize(submeshIndex + 1, AssetHandle(0));
        }
        submeshMaterials[submeshIndex] = materialHandle;
    }

    AssetHandle getSubmeshMaterial(uint32_t submeshIndex) const
    {
        if (submeshIndex < submeshMaterials.size()) {
            return submeshMaterials[submeshIndex];
        }
        return AssetHandle(0);
    }

    void clearSubmeshMaterial(uint32_t submeshIndex)
    {
        if (submeshIndex < submeshMaterials.size()) {
            submeshMaterials[submeshIndex] = AssetHandle(0);
        }
    }

    void clearAllSubmeshMaterials()
    {
        submeshMaterials.clear();
    }

    size_t getSubmeshMaterialCount() const
    {
        return submeshMaterials.size();
    }

    void resizeSubmeshMaterials(size_t count)
    {
        submeshMaterials.resize(count, AssetHandle(0));
    }
};

} // namespace luna
