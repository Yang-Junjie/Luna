#pragma once
#include "vk_types.h"

#include <filesystem>
#include <memory>
#include <optional>

struct GeoSurface {
    uint32_t startIndex{0};
    uint32_t count{0};
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine,
                                                                      const std::filesystem::path& filePath);
