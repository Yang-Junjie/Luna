#pragma once

#include "RHI/Descriptors.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace image_view_lab {

enum class Page : uint8_t {
    MipView = 0,
    ArrayLayerView,
    Slice3DView
};

enum class Phase5ProbeKind : uint8_t {
    None = 0,
    MipIsolation,
    ArrayLayerIsolation
};

inline uint32_t calculate_theoretical_mip_count(uint32_t width, uint32_t height, uint32_t depth = 1)
{
    uint32_t maxDimension = std::max(width, height);
    maxDimension = std::max(maxDimension, depth);

    uint32_t mipCount = 0;
    do {
        ++mipCount;
        maxDimension >>= 1;
    } while (maxDimension > 0);

    return mipCount;
}

struct ViewRecord {
    luna::ImageViewHandle handle{};
    luna::ImageViewDesc desc{};
    std::string label;
};

struct MipViewState {
    uint32_t width = 256;
    uint32_t height = 256;
    uint32_t mipLevels = calculate_theoretical_mip_count(256, 256);
    uint32_t createBaseMip = 1;
    uint32_t createMipCount = 1;
    float previewLod = 0.0f;
    int selectedView = 0;
    int deleteView = -1;
    bool recreateImageRequested = true;
    bool createViewRequested = true;
    bool renderSelectedViewRequested = true;
    bool phase5StampRequested = false;
    uint32_t renderStamp = 0;
    std::vector<ViewRecord> views;
    std::string status;
};

struct ArrayLayerViewState {
    uint32_t width = 128;
    uint32_t height = 128;
    uint32_t mipLevels = calculate_theoretical_mip_count(128, 128);
    uint32_t arrayLayers = 4;
    luna::ImageViewType createType = luna::ImageViewType::Image2D;
    uint32_t createBaseMip = 0;
    uint32_t createMipCount = 1;
    uint32_t createBaseLayer = 0;
    uint32_t createLayerCount = 1;
    int previewLayer = 0;
    float previewLod = 0.0f;
    int selectedView = 0;
    int deleteView = -1;
    bool recreateImageRequested = true;
    bool createViewRequested = false;
    std::vector<ViewRecord> views;
    std::string status;
};

struct Slice3DViewState {
    uint32_t width = 64;
    uint32_t height = 64;
    uint32_t depth = 16;
    uint32_t mipLevels = calculate_theoretical_mip_count(64, 64, 16);
    uint32_t createBaseMip = 0;
    uint32_t createMipCount = 1;
    int previewSlice = 0;
    float previewLod = 0.0f;
    int selectedView = 0;
    int deleteView = -1;
    bool recreateImageRequested = true;
    bool createViewRequested = false;
    std::vector<ViewRecord> views;
    std::string status;
};

struct Phase5ProbeState {
    Phase5ProbeKind request = Phase5ProbeKind::None;
    Phase5ProbeKind completed = Phase5ProbeKind::None;
    bool ready = false;
    bool passed = false;
    uint32_t observedPrimary = 0;
    uint32_t observedSecondary = 0;
    uint32_t expectedPrimary = 0;
    uint32_t expectedSecondary = 0;
    std::string summary;
};

struct State {
    Page page = Page::MipView;
    MipViewState mip;
    ArrayLayerViewState array;
    Slice3DViewState volume;
    Phase5ProbeState phase5Probe;
};

} // namespace image_view_lab
