#include "Imgui/ImGuiContext.h"
#include "RenderDebugPanel.h"

#include <algorithm>
#include <array>
#include <imgui.h>

namespace luna {
namespace {

struct DebugModeItem {
    RenderDebugViewMode mode;
    const char* label;
};

constexpr std::array<DebugModeItem, 3> kDebugModes{{
    {RenderDebugViewMode::None, "None"},
    {RenderDebugViewMode::Velocity, "Velocity"},
    {RenderDebugViewMode::HistoryValidity, "History Validity"},
}};

int modeIndex(RenderDebugViewMode mode)
{
    for (int index = 0; index < static_cast<int>(kDebugModes.size()); ++index) {
        if (kDebugModes[static_cast<size_t>(index)].mode == mode) {
            return index;
        }
    }
    return 0;
}

ImVec2 fitImageSize(uint32_t width, uint32_t height, ImVec2 available)
{
    if (width == 0 || height == 0 || available.x <= 0.0f || available.y <= 0.0f) {
        return ImVec2(0.0f, 0.0f);
    }

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    float image_width = available.x;
    float image_height = image_width / aspect;
    if (image_height > available.y) {
        image_height = available.y;
        image_width = image_height * aspect;
    }

    return ImVec2((std::max)(image_width, 1.0f), (std::max)(image_height, 1.0f));
}

} // namespace

void RenderDebugPanel::onImGuiRender(bool& open, Renderer& renderer)
{
    if (!open) {
        if (renderer.getRenderDebugViewMode() != RenderDebugViewMode::None) {
            renderer.setRenderDebugViewMode(RenderDebugViewMode::None);
        }
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(640.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Render Debug", &open)) {
        ImGui::End();
        if (!open) {
            renderer.setRenderDebugViewMode(RenderDebugViewMode::None);
        }
        return;
    }

    int selected_mode = modeIndex(renderer.getRenderDebugViewMode());
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("View", kDebugModes[static_cast<size_t>(selected_mode)].label)) {
        for (int index = 0; index < static_cast<int>(kDebugModes.size()); ++index) {
            const bool selected = index == selected_mode;
            if (ImGui::Selectable(kDebugModes[static_cast<size_t>(index)].label, selected)) {
                selected_mode = index;
                renderer.setRenderDebugViewMode(kDebugModes[static_cast<size_t>(index)].mode);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (renderer.getRenderDebugViewMode() == RenderDebugViewMode::Velocity) {
        float velocity_scale = renderer.getRenderDebugVelocityScale();
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderFloat("Velocity Scale", &velocity_scale, 1.0f, 200.0f, "%.1f")) {
            renderer.setRenderDebugVelocityScale(velocity_scale);
        }
    }

    ImGui::Separator();

    const auto& debug_texture = renderer.getRenderDebugOutputTexture();
    if (renderer.getRenderDebugViewMode() == RenderDebugViewMode::None) {
        ImGui::TextUnformatted("Select a debug view to render a preview.");
        ImGui::End();
        return;
    }

    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(debug_texture);
    if (texture_id == 0 || !debug_texture) {
        ImGui::TextUnformatted("Debug texture will appear after the next rendered frame.");
        ImGui::End();
        return;
    }

    ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 image_size = fitImageSize(debug_texture->GetWidth(), debug_texture->GetHeight(), available);
    if (image_size.x <= 0.0f || image_size.y <= 0.0f) {
        ImGui::End();
        return;
    }

    const bool flip_uv_y = renderer.getCapabilities().conventions.imgui_render_target_requires_uv_y_flip;
    const ImVec2 uv0(0.0f, flip_uv_y ? 1.0f : 0.0f);
    const ImVec2 uv1(1.0f, flip_uv_y ? 0.0f : 1.0f);

    ImGui::Image(texture_id, image_size, uv0, uv1);
    ImGui::End();
    if (!open) {
        renderer.setRenderDebugViewMode(RenderDebugViewMode::None);
    }
}

} // namespace luna
