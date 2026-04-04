#pragma once

#include "RHI/CommandContext.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sync_lab {

enum class Page : uint8_t {
    HistoryCopy = 0,
    Readback,
    Indirect
};

struct TimelineEntry {
    uint64_t serial = 0;
    std::string label;
};

struct HistoryCopyState {
    bool advanceFrameRequested = true;
    bool autoAdvance = false;
    bool pauseCopy = false;
    bool runBarrierOnlyRequested = false;
    int sampleFrame = 0;

    luna::ImageLayout barrierOldLayout = luna::ImageLayout::ShaderReadOnly;
    luna::ImageLayout barrierNewLayout = luna::ImageLayout::TransferDst;
    luna::PipelineStage barrierSrcStage = luna::PipelineStage::FragmentShader;
    luna::PipelineStage barrierDstStage = luna::PipelineStage::Transfer;
    luna::ResourceAccess barrierSrcAccess = luna::ResourceAccess::ShaderRead;
    luna::ResourceAccess barrierDstAccess = luna::ResourceAccess::TransferWrite;

    std::string barrierSummary;
    std::string status;
};

struct ReadbackState {
    bool copyBufferToImageRequested = true;
    bool copyImageToBufferRequested = false;
    int regionX = 0;
    int regionY = 0;
    bool hasReadbackData = false;
    std::array<uint32_t, 16> pixels{};
    std::string status;
};

struct IndirectState {
    bool generateArgsRequested = true;
    bool runRequested = true;
    bool useIndirect = true;
    int desiredGroupCountX = 12;
    int desiredGroupCountY = 8;
    std::array<uint32_t, 3> gpuArgs = {0, 0, 0};
    std::string status;
};

struct State {
    Page page = Page::HistoryCopy;
    HistoryCopyState history;
    ReadbackState readback;
    IndirectState indirect;
    std::vector<TimelineEntry> timeline;
};

} // namespace sync_lab
