#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"
#include "RenderProfilerPlugin.h"

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.render-profiler";
constexpr const char* kWindowId = "luna.editor.render-profiler.window";
constexpr size_t kMaxHistoryFrames = 120;

std::string formatFloat(double value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string formatUInt(uint32_t value)
{
    return std::to_string(value);
}

void appendHistory(std::vector<luna::editor::RenderGraphProfileSnapshot>& history,
                   const luna::editor::RenderGraphProfileSnapshot& profile)
{
    if (profile.passes.empty()) {
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

double averageGraphCpuMs(const std::vector<luna::editor::RenderGraphProfileSnapshot>& history, int average_frames)
{
    if (history.empty()) {
        return 0.0;
    }

    double total_ms = 0.0;
    size_t sample_count = 0;
    const size_t begin_index = averageBeginIndex(history.size(), average_frames);
    for (size_t snapshot_index = begin_index; snapshot_index < history.size(); ++snapshot_index) {
        total_ms += history[snapshot_index].total_cpu_time_ms;
        ++sample_count;
    }

    return sample_count > 0 ? total_ms / static_cast<double>(sample_count) : 0.0;
}

double averageGraphGpuMs(const std::vector<luna::editor::RenderGraphProfileSnapshot>& history, int average_frames)
{
    if (history.empty()) {
        return 0.0;
    }

    double total_ms = 0.0;
    size_t sample_count = 0;
    const size_t begin_index = averageBeginIndex(history.size(), average_frames);
    for (size_t snapshot_index = begin_index; snapshot_index < history.size(); ++snapshot_index) {
        const auto& snapshot = history[snapshot_index];
        if (snapshot.gpu_timing_pending || !snapshot.gpu_timing_supported) {
            continue;
        }

        total_ms += snapshot.total_gpu_time_ms;
        ++sample_count;
    }

    return sample_count > 0 ? total_ms / static_cast<double>(sample_count) : 0.0;
}

double averagePassCpuMs(std::string_view pass_name,
                        const std::vector<luna::editor::RenderGraphProfileSnapshot>& history,
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
            std::find_if(snapshot.passes.begin(), snapshot.passes.end(), [pass_name](const auto& pass) {
                return pass.name == pass_name;
            });
        if (pass_it == snapshot.passes.end()) {
            continue;
        }

        total_ms += pass_it->cpu_time_ms;
        ++sample_count;
    }

    return sample_count > 0 ? total_ms / static_cast<double>(sample_count) : 0.0;
}

double averagePassGpuMs(std::string_view pass_name,
                        const std::vector<luna::editor::RenderGraphProfileSnapshot>& history,
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
            std::find_if(snapshot.passes.begin(), snapshot.passes.end(), [pass_name](const auto& pass) {
                return pass.name == pass_name;
            });
        if (pass_it == snapshot.passes.end() || !pass_it->has_gpu_time) {
            continue;
        }

        total_ms += pass_it->gpu_time_ms;
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

std::string joinText(std::initializer_list<std::string_view> parts)
{
    std::string result;
    for (std::string_view part : parts) {
        if (!result.empty()) {
            result += ' ';
        }
        result += part;
    }
    return result;
}

std::string gpuMetricValue(const luna::editor::RenderGraphProfileSnapshot& profile)
{
    if (!profile.gpu_timing_supported) {
        return "Unavailable";
    }
    if (profile.gpu_timing_pending) {
        return "Pending";
    }
    return formatFloat(profile.total_gpu_time_ms, 3) + " ms";
}

luna::editor::StatusVariant gpuMetricVariant(const luna::editor::RenderGraphProfileSnapshot& profile)
{
    if (!profile.gpu_timing_supported) {
        return luna::editor::StatusVariant::Warning;
    }
    if (profile.gpu_timing_pending) {
        return luna::editor::StatusVariant::Info;
    }
    return luna::editor::StatusVariant::Success;
}

class RenderProfilerPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Render Profiler",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Render Profiler",
            .default_open = false,
            .default_size = luna::editor::Vec2{.x = 1120.0f, .y = 460.0f},
            .draw =
                [this](luna::editor::WindowDrawContext& context) {
                    luna::editor::Host& host = context.host();
                    luna::editor::Ui& ui = context.ui();
                    const bool profiling_enabled = host.rendering().isRenderGraphProfilingEnabled();
                    if (profiling_enabled) {
                        m_display_profile = host.rendering().renderGraphProfile();
                        appendHistory(m_history, m_display_profile);
                    }

                    const luna::editor::RenderGraphProfileSnapshot& profile = m_display_profile;
                    ui.heading("Render Profiler",
                               profiling_enabled ? std::string("Profiling enabled") : std::string("Profiling stopped"));
                    ui.beginPanel("##RenderProfilerToolbar");
                    if (profiling_enabled) {
                        if (ui.button("Stop Profile")) {
                            host.rendering().setRenderGraphProfilingEnabled(false);
                        }
                    } else {
                        if (ui.button("Start Profile", luna::editor::ButtonVariant::Primary)) {
                            m_display_profile = {};
                            m_history.clear();
                            m_last_export_path.clear();
                            m_export_status.clear();
                            host.rendering().setRenderGraphProfilingEnabled(true);
                        }
                    }
                    ui.sameLine();
                    ui.checkbox("Sort by CPU", m_sort_by_cpu);
                    ui.sameLine();
                    ui.checkbox("Sort by GPU", m_sort_by_gpu);
                    ui.sameLine();
                    ui.sliderInt("Average Frames", m_average_frames, 1, static_cast<int>(kMaxHistoryFrames));
                    ui.sameLine();
                    if (ui.button("Clear History", luna::editor::ButtonVariant::Subtle)) {
                        m_history.clear();
                    }
                    ui.sameLine();
                    if (ui.button("Export Trace JSON", luna::editor::ButtonVariant::Subtle)) {
                        const std::filesystem::path export_path =
                            host.rendering().defaultRenderProfileExportPath(host.rendering().backendName());
                        std::string error_message;
                        if (host.rendering().exportRenderGraphProfileChromeTraceJson(
                                profile, export_path, &error_message)) {
                            m_last_export_path = export_path;
                            m_export_status = "Exported trace JSON";
                        } else {
                            m_last_export_path.clear();
                            m_export_status = "Export failed: " + error_message;
                        }
                    }
                    ui.endPanel();

                    m_average_frames = (std::max) (m_average_frames, 1);
                    m_average_frames = (std::min) (m_average_frames, static_cast<int>(kMaxHistoryFrames));
                    const double average_total_cpu_ms =
                        !m_history.empty() ? averageGraphCpuMs(m_history, m_average_frames) : profile.total_cpu_time_ms;
                    const double average_total_gpu_ms =
                        !m_history.empty() ? averageGraphGpuMs(m_history, m_average_frames) : profile.total_gpu_time_ms;

                    if (ui.beginTable(
                            "##RenderProfilerMetrics",
                            4,
                            static_cast<luna::editor::TableFlags>(luna::editor::TableFlag::SizingStretchProp))) {
                        ui.tableNextRow();
                        ui.tableNextColumn();
                        ui.metric("CPU",
                                  formatFloat(profile.total_cpu_time_ms, 3) + " ms",
                                  "Avg " + formatFloat(average_total_cpu_ms, 3) + " ms",
                                  luna::editor::StatusVariant::Info);
                        ui.tableNextColumn();
                        ui.metric("GPU",
                                  gpuMetricValue(profile),
                                  profile.gpu_timing_supported && !profile.gpu_timing_pending
                                      ? "Avg " + formatFloat(average_total_gpu_ms, 3) + " ms"
                                      : std::string{},
                                  gpuMetricVariant(profile));
                        ui.tableNextColumn();
                        ui.metric("Passes",
                                  formatUInt(static_cast<uint32_t>(profile.passes.size())),
                                  "Textures " + formatUInt(profile.texture_count),
                                  luna::editor::StatusVariant::Neutral);
                        ui.tableNextColumn();
                        ui.metric("Samples",
                                  formatUInt(static_cast<uint32_t>(m_history.size())),
                                  "Barriers " + formatUInt(profile.final_barrier_count),
                                  luna::editor::StatusVariant::Neutral);
                        ui.endTable();
                    }
                    if (!m_export_status.empty()) {
                        ui.textDisabled(m_export_status);
                        if (!m_last_export_path.empty()) {
                            ui.textDisabled(m_last_export_path.generic_string());
                        }
                    }
                    ui.separator();

                    if (profile.passes.empty()) {
                        ui.emptyState("No render graph profile data",
                                      "Start profiling and render a frame to populate this table.");
                        return;
                    }

                    std::vector<size_t> pass_indices;
                    pass_indices.reserve(profile.passes.size());
                    for (size_t pass_index = 0; pass_index < profile.passes.size(); ++pass_index) {
                        pass_indices.push_back(pass_index);
                    }

                    if (m_sort_by_gpu) {
                        std::sort(pass_indices.begin(), pass_indices.end(), [&profile](size_t lhs, size_t rhs) {
                            return profile.passes[lhs].gpu_time_ms > profile.passes[rhs].gpu_time_ms;
                        });
                    } else if (m_sort_by_cpu) {
                        std::sort(pass_indices.begin(), pass_indices.end(), [&profile](size_t lhs, size_t rhs) {
                            return profile.passes[lhs].cpu_time_ms > profile.passes[rhs].cpu_time_ms;
                        });
                    }

                    const luna::editor::TableFlags table_flags =
                        luna::editor::TableFlag::BordersInnerV | luna::editor::TableFlag::RowBg |
                        luna::editor::TableFlag::SizingStretchProp | luna::editor::TableFlag::ScrollY;
                    if (ui.beginTable("##RenderGraphPassProfile", 13, table_flags)) {
                        ui.tableSetupColumn(
                            "Pass",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthStretch),
                            0.35f);
                        ui.tableSetupColumn(
                            "Type",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            72.0f);
                        ui.tableSetupColumn(
                            "CPU ms",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            76.0f);
                        ui.tableSetupColumn(
                            "CPU Avg",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            76.0f);
                        ui.tableSetupColumn(
                            "CPU %",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            54.0f);
                        ui.tableSetupColumn(
                            "GPU ms",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            76.0f);
                        ui.tableSetupColumn(
                            "GPU Avg",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            76.0f);
                        ui.tableSetupColumn(
                            "GPU %",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            54.0f);
                        ui.tableSetupColumn(
                            "Size",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            90.0f);
                        ui.tableSetupColumn(
                            "Reads",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            54.0f);
                        ui.tableSetupColumn(
                            "Writes",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            54.0f);
                        ui.tableSetupColumn(
                            "Colors",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            54.0f);
                        ui.tableSetupColumn(
                            "Barriers",
                            static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                            62.0f);
                        ui.tableHeadersRow();

                        for (const size_t pass_index : pass_indices) {
                            const auto& pass = profile.passes[pass_index];
                            const double pass_average_cpu_ms =
                                !m_history.empty() ? averagePassCpuMs(pass.name, m_history, m_average_frames)
                                                   : pass.cpu_time_ms;
                            const float pass_percent = passCpuPercent(pass.cpu_time_ms, profile.total_cpu_time_ms);
                            const double pass_average_gpu_ms =
                                !m_history.empty() ? averagePassGpuMs(pass.name, m_history, m_average_frames)
                                                   : pass.gpu_time_ms;
                            const float gpu_percent =
                                pass.has_gpu_time ? passGpuPercent(pass.gpu_time_ms, profile.total_gpu_time_ms) : 0.0f;

                            ui.tableNextRow();
                            ui.tableNextColumn();
                            ui.text(pass.name);
                            ui.tableNextColumn();
                            ui.text(pass.type);
                            ui.tableNextColumn();
                            ui.text(formatFloat(pass.cpu_time_ms, 3));
                            ui.tableNextColumn();
                            ui.text(formatFloat(pass_average_cpu_ms, 3));
                            ui.tableNextColumn();
                            ui.text(formatFloat(pass_percent, 1));
                            ui.tableNextColumn();
                            ui.text(pass.has_gpu_time ? formatFloat(pass.gpu_time_ms, 3) : std::string("-"));
                            ui.tableNextColumn();
                            ui.text(pass.has_gpu_time ? formatFloat(pass_average_gpu_ms, 3) : std::string("-"));
                            ui.tableNextColumn();
                            ui.text(pass.has_gpu_time ? formatFloat(gpu_percent, 1) : std::string("-"));
                            ui.tableNextColumn();
                            ui.text(joinText(
                                {formatUInt(pass.framebuffer_width), "x", formatUInt(pass.framebuffer_height)}));
                            ui.tableNextColumn();
                            ui.text(formatUInt(pass.read_texture_count));
                            ui.tableNextColumn();
                            ui.text(formatUInt(pass.write_texture_count));
                            ui.tableNextColumn();
                            std::string color_attachment_label = formatUInt(pass.color_attachment_count);
                            if (pass.has_depth_attachment) {
                                color_attachment_label += "+D";
                            }
                            ui.text(color_attachment_label);
                            ui.tableNextColumn();
                            ui.text(formatUInt(pass.pre_barrier_count));
                        }

                        ui.endTable();
                    }
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.rendering().setRenderGraphProfilingEnabled(false);
        host.windows().unregisterWindow(kWindowId);
    }

    void onUpdate(luna::editor::Host& host, float) override
    {
        if (!host.windows().isWindowOpen(kWindowId) && host.rendering().isRenderGraphProfilingEnabled()) {
            host.rendering().setRenderGraphProfilingEnabled(false);
        }
    }

private:
    bool m_sort_by_cpu{true};
    bool m_sort_by_gpu{false};
    int m_average_frames{30};
    luna::editor::RenderGraphProfileSnapshot m_display_profile{};
    std::vector<luna::editor::RenderGraphProfileSnapshot> m_history;
    std::filesystem::path m_last_export_path;
    std::string m_export_status;
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createRenderProfilerPlugin()
{
    return std::make_unique<RenderProfilerPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kRenderProfilerPluginRegistration{
    kPluginId,
    createRenderProfilerPlugin,
};

} // namespace

} // namespace luna::editor
