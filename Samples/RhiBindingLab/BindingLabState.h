#pragma once

#include "RHI/Descriptors.h"

#include <array>
#include <cstdint>
#include <string>

namespace binding_lab {

enum class Page : uint8_t {
    MultiSet = 0,
    DescriptorArray,
    DynamicUniform
};

struct MultiSetState {
    bool includeSet0 = true;
    bool includeSet1 = true;
    bool includeSet2 = true;
    bool buildLayoutRequested = true;
    bool conflictTestRequested = false;

    bool bindGlobal = true;
    bool bindMaterial = true;
    bool bindObject = true;

    std::array<float, 4> globalTint = {1.0f, 0.80f, 0.75f, 1.0f};
    std::array<float, 4> materialColor = {0.18f, 0.72f, 0.92f, 1.0f};
    std::array<float, 2> objectOffset = {0.00f, 0.00f};

    std::array<std::string, 3> layoutSummaries;
    std::string layoutStatus;
    std::string sampleStatus;
};

struct DescriptorArrayState {
    int textureIndex = 0;
    bool replaceSlotRequested = false;
    bool slot2UsesAlternate = false;
    std::array<std::string, 4> slotLabels;
    std::string status;
};

struct DynamicUniformState {
    int objectIndex = 0;
    uint32_t dynamicOffset = 0;
    std::string status;
};

struct State {
    Page page = Page::MultiSet;
    MultiSetState multiSet;
    DescriptorArrayState descriptorArray;
    DynamicUniformState dynamicUniform;
};

} // namespace binding_lab
