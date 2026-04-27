#pragma once

#include "Renderer/RenderGraph.h"

#include <vector>

namespace luna {

class RenderProfilerPanel {
public:
    void onImGuiRender(bool& open, const RenderGraphProfileSnapshot& latest_profile);

private:
    bool m_paused{false};
    bool m_sort_by_cpu{true};
    int m_average_frames{30};
    RenderGraphProfileSnapshot m_display_profile{};
    std::vector<RenderGraphProfileSnapshot> m_history;
};

} // namespace luna
