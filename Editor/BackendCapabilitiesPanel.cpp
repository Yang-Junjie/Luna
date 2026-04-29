#include "BackendCapabilitiesPanel.h"
#include "Renderer/RendererUtilities.h"

#include <imgui.h>

namespace luna {
namespace {

const char* boolText(bool value)
{
    return value ? "Yes" : "No";
}

void capabilityRow(const char* name, bool value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(boolText(value));
}

void textRow(const char* name, const char* value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value);
}

} // namespace

void BackendCapabilitiesPanel::onImGuiRender(bool& open, const Renderer& renderer)
{
    if (!open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(430.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Backend Capabilities", &open)) {
        ImGui::End();
        return;
    }

    const auto& capabilities = renderer.getCapabilities();
    const auto& conventions = capabilities.conventions;

    ImGui::Text("Backend: %s", renderer_detail::backendTypeToString(capabilities.backend_type));
    ImGui::Separator();

    if (ImGui::BeginTable("BackendCapabilitiesTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Capability", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableHeadersRow();

        capabilityRow("Default Render Flow", capabilities.supports_default_render_flow);
        capabilityRow("ImGui", capabilities.supports_imgui);
        capabilityRow("Scene Pick Readback", capabilities.supports_scene_pick_readback);
        capabilityRow("GPU Timestamp", capabilities.supports_gpu_timestamp);
        textRow("GPU Timestamp Mode",
                capabilities.supports_gpu_timestamp
                    ? (capabilities.gpu_timestamp_uses_disjoint_query ? "Disjoint query" : "Fixed period")
                    : "Unavailable");

        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::BeginTable(
            "BackendResourceCapabilitiesTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableHeadersRow();

        capabilityRow("Graphics Pipeline", capabilities.supports_graphics_pipeline);
        capabilityRow("Compute Pipeline", capabilities.supports_compute_pipeline);
        capabilityRow("Sampled Texture", capabilities.supports_sampled_texture);
        capabilityRow("Storage Texture", capabilities.supports_storage_texture);
        capabilityRow("Color Attachment", capabilities.supports_color_attachment);
        capabilityRow("Depth Attachment", capabilities.supports_depth_attachment);
        capabilityRow("Uniform Buffer", capabilities.supports_uniform_buffer);
        capabilityRow("Storage Buffer", capabilities.supports_storage_buffer);
        capabilityRow("Sampler", capabilities.supports_sampler);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("BackendConventionsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Convention", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableHeadersRow();

        capabilityRow("Projection Y Flip", conventions.requires_projection_y_flip);
        textRow("ImGui Clip Top Y", conventions.imgui_clip_top_y_is_negative_one ? "-1" : "+1");
        textRow("ImGui Render Target UV", conventions.imgui_render_target_requires_uv_y_flip ? "Flip Y" : "Direct");
        textRow("Scene Pick Y", conventions.scene_pick_y_matches_display_y ? "Displayed Y" : "Invert displayed Y");

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace luna
