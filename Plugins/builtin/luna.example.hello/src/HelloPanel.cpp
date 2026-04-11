#include "HelloPanel.h"

#include "imgui.h"

namespace luna::example {

void HelloPanel::onImGuiRender()
{
    ImGui::TextUnformatted("Hello from luna.example.hello.");
    ImGui::Separator();
    ImGui::TextUnformatted("This is the concrete sample plugin referenced by the plugin docs.");
    ImGui::BulletText("Plugin id: luna.example.hello");
    ImGui::BulletText("Contribution type: Editor Panel");
    ImGui::BulletText("Host: editor");
}

} // namespace luna::example
