#pragma once

#include "EditorApi/EditorTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace luna::editor {

struct RenderingBackendEntry {
    std::string name;
    std::string status;
    bool current{false};
    bool default_backend{false};
};

struct RenderingBackendConventions {
    bool requires_projection_y_flip{false};
    bool imgui_clip_top_y_is_negative_one{false};
    bool imgui_render_target_requires_uv_y_flip{false};
    bool scene_pick_y_matches_display_y{false};
};

struct RenderingBackendCapabilities {
    std::string active_backend_name;
    std::string compiled_backend_names;
    std::vector<RenderingBackendEntry> compiled_backends;

    bool supports_default_render_flow{false};
    bool supports_imgui{false};
    bool supports_scene_pick_readback{false};
    bool supports_gpu_timestamp{false};
    bool gpu_timestamp_uses_disjoint_query{false};
    bool supports_graphics_pipeline{false};
    bool supports_compute_pipeline{false};
    bool supports_sampled_texture{false};
    bool supports_storage_texture{false};
    bool supports_color_attachment{false};
    bool supports_depth_attachment{false};
    bool supports_uniform_buffer{false};
    bool supports_storage_buffer{false};
    bool supports_sampler{false};

    RenderingBackendConventions conventions{};
};

struct RenderGraphPassProfile {
    std::string name;
    std::string type;
    double cpu_time_ms{0.0};
    double gpu_time_ms{0.0};
    bool has_gpu_time{false};
    uint32_t framebuffer_width{0};
    uint32_t framebuffer_height{0};
    uint32_t read_texture_count{0};
    uint32_t write_texture_count{0};
    uint32_t color_attachment_count{0};
    bool has_depth_attachment{false};
    uint32_t pre_barrier_count{0};
};

struct RenderGraphProfileSnapshot {
    uint64_t frame_index{0};
    double total_cpu_time_ms{0.0};
    double total_gpu_time_ms{0.0};
    bool gpu_timing_supported{false};
    bool gpu_timing_pending{false};
    uint32_t texture_count{0};
    uint32_t final_barrier_count{0};
    std::vector<RenderGraphPassProfile> passes;
};

class RenderingService {
public:
    virtual ~RenderingService() = default;

    virtual std::string backendName() const = 0;
    virtual RenderingBackendCapabilities backendCapabilities() const = 0;
    virtual RenderGraphProfileSnapshot renderGraphProfile() const = 0;
    virtual bool isRenderGraphProfilingEnabled() const noexcept = 0;
    virtual void setRenderGraphProfilingEnabled(bool enabled) = 0;
    virtual std::filesystem::path defaultRenderProfileExportPath(std::string_view backend_name = {}) const = 0;
    virtual bool exportRenderGraphProfileChromeTraceJson(const RenderGraphProfileSnapshot& profile,
                                                         const std::filesystem::path& output_path,
                                                         std::string* error_message = nullptr) const = 0;
    virtual float frameTimeMilliseconds() const noexcept = 0;
    virtual float framesPerSecond() const noexcept = 0;
    virtual UVec2 sceneOutputSize() const noexcept = 0;
};

} // namespace luna::editor
