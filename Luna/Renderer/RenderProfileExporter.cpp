#include "Renderer/RenderProfileExporter.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace luna {
namespace {

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

void writeJsonEscaped(std::ostream& output, std::string_view value)
{
    output << '"';
    for (const char c : value) {
        switch (c) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(static_cast<unsigned char>(c)) << std::dec << std::setfill(' ');
                } else {
                    output << c;
                }
                break;
        }
    }
    output << '"';
}

double millisecondsToMicroseconds(double milliseconds)
{
    return milliseconds * 1000.0;
}

void writeTraceMetadataEvent(std::ostream& output, int pid, int tid, std::string_view name, std::string_view value)
{
    output << "{\"name\":";
    writeJsonEscaped(output, name);
    output << ",\"ph\":\"M\",\"pid\":" << pid << ",\"tid\":" << tid << ",\"args\":{\"name\":";
    writeJsonEscaped(output, value);
    output << "}}";
}

void writePassTraceEvent(std::ostream& output,
                         const RenderGraphPassProfile& pass,
                         const RenderProfileExportOptions& options,
                         int tid,
                         bool gpu_event)
{
    output << "{\"name\":";
    writeJsonEscaped(output, pass.Name);
    output << ",\"cat\":\"RenderGraph\",\"ph\":\"X\",\"pid\":1,\"tid\":" << tid;
    output << ",\"ts\":" << std::fixed << std::setprecision(3)
           << millisecondsToMicroseconds(gpu_event ? pass.GpuStartMs : pass.CpuStartMs);
    output << ",\"dur\":" << std::fixed << std::setprecision(3)
           << millisecondsToMicroseconds(gpu_event ? pass.GpuTimeMs : pass.CpuTimeMs);
    output << ",\"args\":{";
    output << "\"frame\":" << options.frame_index;
    output << ",\"backend\":";
    writeJsonEscaped(output, options.backend_name);
    output << ",\"type\":";
    writeJsonEscaped(output, renderGraphPassTypeToString(pass.Type));
    output << ",\"lane\":";
    writeJsonEscaped(output, gpu_event ? "GPU" : "CPU");
    output << ",\"cpu_ms\":" << std::fixed << std::setprecision(6) << pass.CpuTimeMs;
    output << ",\"gpu_ms\":";
    if (pass.HasGpuTime) {
        output << std::fixed << std::setprecision(6) << pass.GpuTimeMs;
    } else {
        output << "null";
    }
    output << ",\"reads\":" << pass.ReadTextureCount;
    output << ",\"writes\":" << pass.WriteTextureCount;
    output << ",\"colors\":" << pass.ColorAttachmentCount;
    output << ",\"has_depth\":" << (pass.HasDepthAttachment ? "true" : "false");
    output << ",\"barriers\":" << pass.PreBarrierCount;
    output << ",\"width\":" << pass.FramebufferWidth;
    output << ",\"height\":" << pass.FramebufferHeight;
    output << "}}";
}

std::string sanitizeFileToken(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char c : value) {
        const bool alpha_numeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        result.push_back(alpha_numeric ? c : '_');
    }
    return result;
}

} // namespace

bool exportRenderGraphProfileChromeTraceJson(const RenderGraphProfileSnapshot& profile,
                                             const std::filesystem::path& output_path,
                                             const RenderProfileExportOptions& options,
                                             std::string* error_message)
{
    if (output_path.empty()) {
        if (error_message != nullptr) {
            *error_message = "Output path is empty";
        }
        return false;
    }

    std::error_code ec;
    const auto parent_path = output_path.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path, ec);
        if (ec) {
            if (error_message != nullptr) {
                *error_message = "Failed to create output directory: " + ec.message();
            }
            return false;
        }
    }

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to open output file";
        }
        return false;
    }

    output << "{\"traceEvents\":[";
    bool first_event = true;
    const auto append_separator = [&]() {
        if (!first_event) {
            output << ',';
        }
        first_event = false;
    };

    append_separator();
    writeTraceMetadataEvent(output, 1, 1, "thread_name", "CPU RenderGraph");
    append_separator();
    writeTraceMetadataEvent(output, 1, 2, "thread_name", "GPU Graphics Queue");
    append_separator();
    writeTraceMetadataEvent(output, 1, 0, "process_name", options.trace_name);

    for (const auto& pass : profile.Passes) {
        append_separator();
        writePassTraceEvent(output, pass, options, 1, false);

        if (pass.HasGpuTime) {
            append_separator();
            writePassTraceEvent(output, pass, options, 2, true);
        }
    }

    output << "],\"metadata\":{";
    output << "\"trace_name\":";
    writeJsonEscaped(output, options.trace_name);
    output << ",\"backend\":";
    writeJsonEscaped(output, options.backend_name);
    output << ",\"frame\":" << options.frame_index;
    output << ",\"total_cpu_ms\":" << std::fixed << std::setprecision(6) << profile.TotalCpuTimeMs;
    output << ",\"total_gpu_ms\":" << std::fixed << std::setprecision(6) << profile.TotalGpuTimeMs;
    output << ",\"gpu_timing_supported\":" << (profile.GpuTimingSupported ? "true" : "false");
    output << ",\"gpu_timing_pending\":" << (profile.GpuTimingPending ? "true" : "false");
    output << ",\"textures\":" << profile.TextureCount;
    output << ",\"final_barriers\":" << profile.FinalBarrierCount;
    output << "}}";

    if (!output.good()) {
        if (error_message != nullptr) {
            *error_message = "Failed while writing output file";
        }
        return false;
    }

    return true;
}

std::filesystem::path makeDefaultRenderProfileExportPath(std::string_view backend_name)
{
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    std::ostringstream filename;
    filename << "render_profile";
    if (!backend_name.empty()) {
        filename << "_" << sanitizeFileToken(backend_name);
    }
    filename << "_" << seconds << ".luna_trace.json";

    return std::filesystem::path(LUNA_PROJECT_ROOT) / "build" / "profiles" / filename.str();
}

} // namespace luna
