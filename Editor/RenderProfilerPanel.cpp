#include "RenderProfilerPanel.h"

#include <algorithm>
#include <string_view>

#include <imgui.h>

namespace luna {
namespace {

constexpr size_t kMaxHistoryFrames = 120;

const char* renderGraphPassTypeToString(RenderGraphPassType type)
{
    switch (type) {
        case RenderGraphPassType::Raster:
            return "Raster";
        case RenderGraphPassType::Compute:
            return "Compute";
        default:
            return "Unknown";
    }
}

void appendHistory(std::vector<RenderGraphProfileSnapshot>& history, const RenderGraphProfileSnapshot& profile)
{
    if (profile.Passes.empty()) {
        return;
    }

    history.push_back(profile);
    if (history.size() > kMaxHistoryFrames) {
        history.erase(history.begin());
    }
}

size_t averageBeginIndex(size_t history_size, int average_frames)
{
    const size_t frame_count = static_cast<size_t>((std::max) (average_frames, 1));
    return history_size > frame_count ? history_size - frame_count : 0;
}

double averageGraphCpuMs(const std::vector<RenderGraphProfileSnapshot>& history, int average_frames)
{
    if (history.empty()) {
        return 0.0;
    }

    double total_ms = 0.0;
    size_t sample_count = 0;
    const size_t begin_index = averageBeginIndex(history.size(), average_frames);
    for (size_t snapshot_index = begin_index; snapshot_index < history.size(); ++snapshot_index) {
        total_ms += history[snapshot_index].TotalCpuTimeMs;
        ++sample_count;
    }

    return sample_count > 0 ? total_ms / static_cast<double>(sample_count) : 0.0;
}

double averagePassCpuMs(std::string_view pass_name,
                        const std::vector<RenderGraphProfileSnapshot>& history,
                        int average_frames)
{
    if (history.empty()) {
        return 0.0;
    }

    double total_ms = 0.0;
    size_t sample_count = 0;
    const size_t begin_index = averageBeginIndex(history.size(), average_frames);
    for (size_t snapshot_index = begin_index; snapshot_index < history.size(); ++snapshot_index) {
        const auto& snapshot = history[snapshot_index];
        const auto pass_it = std::find_if(snapshot.Passes.begin(), snapshot.Passes.end(), [pass_name](const auto& pass) {
            return pass.Name == pass_name;
        });
        if (pass_it == snapshot.Passes.end()) {
            continue;
        }

        total_ms += pass_it->CpuTimeMs;
        ++sample_count;
    }

    return sample_count > 0 ? total_ms / static_cast<double>(sample_count) : 0.0;
}

float passCpuPercent(double pass_cpu_ms, double total_cpu_ms)
{
    if (total_cpu_ms <= 0.0) {
        return 0.0f;
    }

    return static_cast<float>((pass_cpu_ms / total_cpu_ms) * 100.0);
}

} // namespace

void RenderProfilerPanel::onImGuiRender(bool& open, const RenderGraphProfileSnapshot& latest_profile)
{
    if (!open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Render Profiler", &open)) {
        ImGui::End();
        return;
    }

    if (!m_paused) {
        m_display_profile = latest_profile;
        appendHistory(m_history, m_display_profile);
    }

    const RenderGraphProfileSnapshot& profile = m_display_profile;
    ImGui::Checkbox("Pause", &m_paused);
    ImGui::SameLine();
    ImGui::Checkbox("Sort by CPU", &m_sort_by_cpu);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderInt("Average Frames", &m_average_frames, 1, static_cast<int>(kMaxHistoryFrames));
    ImGui::SameLine();
    if (ImGui::Button("Clear History")) {
        m_history.clear();
    }

    m_average_frames = std::clamp(m_average_frames, 1, static_cast<int>(kMaxHistoryFrames));
    const double average_total_cpu_ms =
        !m_history.empty() ? averageGraphCpuMs(m_history, m_average_frames) : profile.TotalCpuTimeMs;

    ImGui::Text("RenderGraph CPU: %.3f ms  |  Avg: %.3f ms", profile.TotalCpuTimeMs, average_total_cpu_ms);
    ImGui::Text("Passes: %zu  |  Textures: %u  |  Final Barriers: %u  |  Samples: %zu",
                profile.Passes.size(),
                profile.TextureCount,
                profile.FinalBarrierCount,
                m_history.size());
    ImGui::Separator();

    if (profile.Passes.empty()) {
        ImGui::TextUnformatted("No render graph profile data for the latest frame.");
        ImGui::End();
        return;
    }

    std::vector<size_t> pass_indices;
    pass_indices.reserve(profile.Passes.size());
    for (size_t pass_index = 0; pass_index < profile.Passes.size(); ++pass_index) {
        pass_indices.push_back(pass_index);
    }

    if (m_sort_by_cpu) {
        std::sort(pass_indices.begin(), pass_indices.end(), [&profile](size_t lhs, size_t rhs) {
            return profile.Passes[lhs].CpuTimeMs > profile.Passes[rhs].CpuTimeMs;
        });
    }

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##RenderGraphPassProfile", 10, table_flags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Avg ms", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Reads", ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableSetupColumn("Writes", ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableSetupColumn("Colors", ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableSetupColumn("Barriers", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();

        for (const size_t pass_index : pass_indices) {
            const auto& pass = profile.Passes[pass_index];
            const double pass_average_cpu_ms =
                !m_history.empty() ? averagePassCpuMs(pass.Name, m_history, m_average_frames) : pass.CpuTimeMs;
            const float pass_percent = passCpuPercent(pass.CpuTimeMs, profile.TotalCpuTimeMs);

            ImGui::TableNextRow();
            if (pass_percent >= 25.0f) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::GetColorU32(ImVec4(0.35f, 0.12f, 0.08f, 0.45f)));
            } else if (pass_percent >= 10.0f) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::GetColorU32(ImVec4(0.30f, 0.22f, 0.08f, 0.35f)));
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(pass.Name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(renderGraphPassTypeToString(pass.Type));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", pass.CpuTimeMs);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f", pass_average_cpu_ms);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.1f", pass_percent);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%u x %u", pass.FramebufferWidth, pass.FramebufferHeight);
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%u", pass.ReadTextureCount);
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%u", pass.WriteTextureCount);
            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%u%s", pass.ColorAttachmentCount, pass.HasDepthAttachment ? "+D" : "");
            ImGui::TableSetColumnIndex(9);
            ImGui::Text("%u", pass.PreBarrierCount);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace luna
