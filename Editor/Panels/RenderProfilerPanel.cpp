#include "EditorUI.h"
#include "Renderer/RenderProfileExporter.h"
#include "RenderProfilerPanel.h"

#include <algorithm>
#include <imgui.h>
#include <string_view>

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
    const size_t frame_count = static_cast<size_t>((std::max)(average_frames, 1));
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

double averageGraphGpuMs(const std::vector<RenderGraphProfileSnapshot>& history, int average_frames)
{
    if (history.empty()) {
        return 0.0;
    }

    double total_ms = 0.0;
    size_t sample_count = 0;
    const size_t begin_index = averageBeginIndex(history.size(), average_frames);
    for (size_t snapshot_index = begin_index; snapshot_index < history.size(); ++snapshot_index) {
        const auto& snapshot = history[snapshot_index];
        if (snapshot.GpuTimingPending || !snapshot.GpuTimingSupported) {
            continue;
        }

        total_ms += snapshot.TotalGpuTimeMs;
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
        const auto pass_it =
            std::find_if(snapshot.Passes.begin(), snapshot.Passes.end(), [pass_name](const auto& pass) {
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

double averagePassGpuMs(std::string_view pass_name,
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
        const auto pass_it =
            std::find_if(snapshot.Passes.begin(), snapshot.Passes.end(), [pass_name](const auto& pass) {
                return pass.Name == pass_name;
            });
        if (pass_it == snapshot.Passes.end() || !pass_it->HasGpuTime) {
            continue;
        }

        total_ms += pass_it->GpuTimeMs;
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

float passGpuPercent(double pass_gpu_ms, double total_gpu_ms)
{
    if (total_gpu_ms <= 0.0) {
        return 0.0f;
    }

    return static_cast<float>((pass_gpu_ms / total_gpu_ms) * 100.0);
}

} // namespace

void RenderProfilerPanel::onImGuiRender(bool& open,
                                        const RenderGraphProfileSnapshot& latest_profile,
                                        std::string_view backend_name,
                                        bool profiling_enabled,
                                        const std::function<void(bool)>& set_profiling_enabled)
{
    if (!open) {
        if (profiling_enabled && set_profiling_enabled) {
            set_profiling_enabled(false);
        }
        return;
    }

    ImGui::SetNextWindowSize(editor::ui::scaled(1120.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Render Profiler", &open)) {
        ImGui::End();
        if (!open && profiling_enabled && set_profiling_enabled) {
            set_profiling_enabled(false);
        }
        return;
    }

    if (profiling_enabled) {
        m_display_profile = latest_profile;
        appendHistory(m_history, m_display_profile);
    }

    const RenderGraphProfileSnapshot& profile = m_display_profile;
    if (profiling_enabled) {
        if (ImGui::Button("Stop Profile") && set_profiling_enabled) {
            set_profiling_enabled(false);
        }
    } else {
        if (ImGui::Button("Start Profile") && set_profiling_enabled) {
            m_display_profile = {};
            m_history.clear();
            m_last_export_path.clear();
            m_export_status.clear();
            set_profiling_enabled(true);
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Sort by CPU", &m_sort_by_cpu);
    ImGui::SameLine();
    ImGui::Checkbox("Sort by GPU", &m_sort_by_gpu);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(editor::ui::scale(160.0f));
    ImGui::SliderInt("Average Frames", &m_average_frames, 1, static_cast<int>(kMaxHistoryFrames));
    ImGui::SameLine();
    if (ImGui::Button("Clear History")) {
        m_history.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Trace JSON")) {
        const std::filesystem::path export_path = makeDefaultRenderProfileExportPath(backend_name);
        std::string error_message;
        RenderProfileExportOptions options{
            .trace_name = "Luna RenderGraph",
            .backend_name = std::string(backend_name),
            .frame_index = profile.FrameIndex,
        };
        if (exportRenderGraphProfileChromeTraceJson(profile, export_path, options, &error_message)) {
            m_last_export_path = export_path;
            m_export_status = "Exported trace JSON";
        } else {
            m_last_export_path.clear();
            m_export_status = "Export failed: " + error_message;
        }
    }

    m_average_frames = std::clamp(m_average_frames, 1, static_cast<int>(kMaxHistoryFrames));
    const double average_total_cpu_ms =
        !m_history.empty() ? averageGraphCpuMs(m_history, m_average_frames) : profile.TotalCpuTimeMs;
    const double average_total_gpu_ms =
        !m_history.empty() ? averageGraphGpuMs(m_history, m_average_frames) : profile.TotalGpuTimeMs;

    ImGui::Text("RenderGraph CPU: %.3f ms  |  Avg: %.3f ms", profile.TotalCpuTimeMs, average_total_cpu_ms);
    if (!profile.GpuTimingSupported) {
        ImGui::TextUnformatted("RenderGraph GPU: unavailable");
    } else if (profile.GpuTimingPending) {
        ImGui::TextUnformatted("RenderGraph GPU: pending");
    } else {
        ImGui::Text("RenderGraph GPU: %.3f ms  |  Avg: %.3f ms", profile.TotalGpuTimeMs, average_total_gpu_ms);
    }
    ImGui::Text("Passes: %zu  |  Textures: %u  |  Final Barriers: %u  |  Samples: %zu",
                profile.Passes.size(),
                profile.TextureCount,
                profile.FinalBarrierCount,
                m_history.size());
    if (!m_export_status.empty()) {
        ImGui::TextDisabled("%s", m_export_status.c_str());
        if (!m_last_export_path.empty()) {
            ImGui::TextDisabled("%s", m_last_export_path.generic_string().c_str());
        }
    }
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

    if (m_sort_by_gpu) {
        std::sort(pass_indices.begin(), pass_indices.end(), [&profile](size_t lhs, size_t rhs) {
            return profile.Passes[lhs].GpuTimeMs > profile.Passes[rhs].GpuTimeMs;
        });
    } else if (m_sort_by_cpu) {
        std::sort(pass_indices.begin(), pass_indices.end(), [&profile](size_t lhs, size_t rhs) {
            return profile.Passes[lhs].CpuTimeMs > profile.Passes[rhs].CpuTimeMs;
        });
    }

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##RenderGraphPassProfile", 13, table_flags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(72.0f));
        ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(76.0f));
        ImGui::TableSetupColumn("CPU Avg", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(76.0f));
        ImGui::TableSetupColumn("CPU %", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(54.0f));
        ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(76.0f));
        ImGui::TableSetupColumn("GPU Avg", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(76.0f));
        ImGui::TableSetupColumn("GPU %", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(54.0f));
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(90.0f));
        ImGui::TableSetupColumn("Reads", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(54.0f));
        ImGui::TableSetupColumn("Writes", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(54.0f));
        ImGui::TableSetupColumn("Colors", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(54.0f));
        ImGui::TableSetupColumn("Barriers", ImGuiTableColumnFlags_WidthFixed, editor::ui::scale(62.0f));
        ImGui::TableHeadersRow();

        for (const size_t pass_index : pass_indices) {
            const auto& pass = profile.Passes[pass_index];
            const double pass_average_cpu_ms =
                !m_history.empty() ? averagePassCpuMs(pass.Name, m_history, m_average_frames) : pass.CpuTimeMs;
            const float pass_percent = passCpuPercent(pass.CpuTimeMs, profile.TotalCpuTimeMs);
            const double pass_average_gpu_ms =
                !m_history.empty() ? averagePassGpuMs(pass.Name, m_history, m_average_frames) : pass.GpuTimeMs;
            const float gpu_percent = pass.HasGpuTime ? passGpuPercent(pass.GpuTimeMs, profile.TotalGpuTimeMs) : 0.0f;

            ImGui::TableNextRow();
            const float hot_percent = pass.HasGpuTime ? gpu_percent : pass_percent;
            if (hot_percent >= 25.0f) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::GetColorU32(ImVec4(0.35f, 0.12f, 0.08f, 0.45f)));
            } else if (hot_percent >= 10.0f) {
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
            if (pass.HasGpuTime) {
                ImGui::Text("%.3f", pass.GpuTimeMs);
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableSetColumnIndex(6);
            if (pass.HasGpuTime) {
                ImGui::Text("%.3f", pass_average_gpu_ms);
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableSetColumnIndex(7);
            if (pass.HasGpuTime) {
                ImGui::Text("%.1f", gpu_percent);
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%u x %u", pass.FramebufferWidth, pass.FramebufferHeight);
            ImGui::TableSetColumnIndex(9);
            ImGui::Text("%u", pass.ReadTextureCount);
            ImGui::TableSetColumnIndex(10);
            ImGui::Text("%u", pass.WriteTextureCount);
            ImGui::TableSetColumnIndex(11);
            ImGui::Text("%u%s", pass.ColorAttachmentCount, pass.HasDepthAttachment ? "+D" : "");
            ImGui::TableSetColumnIndex(12);
            ImGui::Text("%u", pass.PreBarrierCount);
        }

        ImGui::EndTable();
    }

    ImGui::End();
    if (!open && profiling_enabled && set_profiling_enabled) {
        set_profiling_enabled(false);
    }
}

} // namespace luna
