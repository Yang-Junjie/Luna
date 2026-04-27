#pragma once

#include "Renderer/RenderGraph.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace luna {

struct RenderProfileExportOptions {
    std::string trace_name{"Luna RenderGraph"};
    std::string backend_name;
    uint64_t frame_index{0};
};

bool exportRenderGraphProfileChromeTraceJson(const RenderGraphProfileSnapshot& profile,
                                             const std::filesystem::path& output_path,
                                             const RenderProfileExportOptions& options = {},
                                             std::string* error_message = nullptr);

std::filesystem::path makeDefaultRenderProfileExportPath(std::string_view backend_name = {});

} // namespace luna
