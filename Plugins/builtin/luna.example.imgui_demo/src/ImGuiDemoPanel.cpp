#include "imgui.h"
#include "ImGuiDemoPanel.h"

namespace luna::example {

void ImGuiDemoPanel::onImGuiRender()
{
    ImGui::TextWrapped("This panel owns Dear ImGui's demo window. Keep this panel open while browsing the demo.");
    ImGui::Checkbox("Show Dear ImGui Demo Window", &m_show_demo_window);
    ImGui::Separator();
    ImGui::TextUnformatted("The demo content comes from third_party/imgui/imgui_demo.cpp.");

    if (m_show_demo_window) {
        ImGui::ShowDemoWindow(&m_show_demo_window);
    }
}

} // namespace luna::example
