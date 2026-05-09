#pragma once

#include "Asset/Asset.h"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace luna {

struct MeshComponent {
    static constexpr uint32_t AllSubmeshes = UINT32_MAX;

    AssetHandle meshHandle = AssetHandle(0);

    uint32_t firstSubmesh = 0;
    uint32_t submeshCount = AllSubmeshes;

    std::vector<AssetHandle> submeshMaterials;

    MeshComponent() = default;

    MeshComponent(const MeshComponent&) = default;

    void setSubmeshRange(uint32_t firstSubmeshIndex, uint32_t count = AllSubmeshes)
    {
        firstSubmesh = firstSubmeshIndex;
        submeshCount = count;
    }

    void resetSubmeshRange()
    {
        firstSubmesh = 0;
        submeshCount = AllSubmeshes;
    }

    size_t resolveSubmeshCount(size_t totalSubmeshes) const
    {
        if (firstSubmesh >= totalSubmeshes) {
            return 0;
        }

        const size_t remaining = totalSubmeshes - firstSubmesh;
        if (submeshCount == AllSubmeshes) {
            return remaining;
        }

        return (std::min)(remaining, static_cast<size_t>(submeshCount));
    }

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
