#pragma once

#include "Renderer/RenderGraph.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace luna {

class RenderProfilerPanel {
public:
    void onImGuiRender(bool& open,
                       const RenderGraphProfileSnapshot& latest_profile,
                       std::string_view backend_name,
                       bool profiling_enabled,
                       const std::function<void(bool)>& set_profiling_enabled);

private:
    bool m_sort_by_cpu{true};
    bool m_sort_by_gpu{false};
    int m_average_frames{30};
    RenderGraphProfileSnapshot m_display_profile{};
    std::vector<RenderGraphProfileSnapshot> m_history;
    std::filesystem::path m_last_export_path;
    std::string m_export_status;
};

} // namespace luna
