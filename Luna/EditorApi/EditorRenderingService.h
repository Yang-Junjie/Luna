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

enum class RenderDebugViewMode : uint8_t {
    None,
    Velocity,
    HistoryValidity,
    ShadowCascades,
    BaseColor,
    Normal,
    Metallic,
    Roughness,
    DirectLighting,
    SpecularIbl,
    BloomInput,
    BloomPrefilter,
    BloomMip0,
    BloomMip1,
    BloomMip2,
    BloomMip3,
    BloomMip4,
    BloomMip5,
    BloomComposite,
};

struct RenderDebugViewModeInfo {
    RenderDebugViewMode mode{RenderDebugViewMode::None};
    std::string label;
};

enum class RenderFeatureGraphResourceKind : uint8_t {
    Texture,
    Buffer,
};

enum class RenderFeatureGraphResourceFlags : uint32_t {
    None = 0,
    Optional = 1 << 0,
    External = 1 << 1,
};

[[nodiscard]] inline RenderFeatureGraphResourceFlags operator|(RenderFeatureGraphResourceFlags lhs,
                                                               RenderFeatureGraphResourceFlags rhs) noexcept
{
    return static_cast<RenderFeatureGraphResourceFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderFeatureGraphResourceFlags& operator|=(RenderFeatureGraphResourceFlags& lhs,
                                                   RenderFeatureGraphResourceFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] inline bool operator&(RenderFeatureGraphResourceFlags lhs, RenderFeatureGraphResourceFlags rhs) noexcept
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

struct RenderFeatureGraphResource {
    std::string name;
    RenderFeatureGraphResourceKind kind{RenderFeatureGraphResourceKind::Texture};
    RenderFeatureGraphResourceFlags flags{RenderFeatureGraphResourceFlags::None};
};

enum class RenderPassResourceAccess : uint8_t {
    Read,
    Write,
    ReadWrite,
};

struct RenderPassResourceUsage {
    std::string name;
    RenderFeatureGraphResourceKind kind{RenderFeatureGraphResourceKind::Texture};
    RenderPassResourceAccess access{RenderPassResourceAccess::Read};
    RenderFeatureGraphResourceFlags flags{RenderFeatureGraphResourceFlags::None};
};

struct RenderFeaturePassInfo {
    std::string name;
    std::vector<RenderPassResourceUsage> resources;
};

struct RenderFeatureStatusEntry {
    std::string name;
    bool ready{false};
};

struct RenderFeatureDiagnostics {
    bool binding_contract_valid{true};
    std::string binding_contract_summary;
    bool pipeline_resources_valid{true};
    std::string pipeline_resources_summary;
    std::vector<RenderFeatureStatusEntry> pipeline_resources;
    bool persistent_resources_valid{true};
    std::string persistent_resources_summary;
    std::vector<RenderFeatureStatusEntry> persistent_resources;
    bool history_resources_valid{true};
    std::string history_resources_summary;
    std::vector<RenderFeatureStatusEntry> history_resources;
};

struct RenderFeatureInfo {
    std::string name;
    std::string display_name;
    std::string category;
    bool enabled{true};
    bool runtime_toggleable{false};
    bool supported{true};
    bool active{true};
    std::string support_summary;
    bool graph_contract_valid{true};
    std::string graph_contract_summary;
    bool pass_contract_valid{true};
    std::string pass_contract_summary;
    std::vector<RenderFeatureGraphResource> graph_inputs;
    std::vector<RenderFeatureGraphResource> graph_outputs;
    std::vector<RenderFeaturePassInfo> passes;
    RenderFeatureDiagnostics diagnostics;
};

enum class RenderFeatureParameterType : uint8_t {
    Bool,
    Int,
    Float,
    Color,
};

struct RenderFeatureParameterValue {
    RenderFeatureParameterType type{RenderFeatureParameterType::Float};
    bool bool_value{false};
    int32_t int_value{0};
    float float_value{0.0f};
    Vec4 color_value{1.0f, 1.0f, 1.0f, 1.0f};
};

struct RenderFeatureParameterInfo {
    std::string name;
    std::string display_name;
    RenderFeatureParameterType type{RenderFeatureParameterType::Float};
    RenderFeatureParameterValue value{};
    RenderFeatureParameterValue min{};
    RenderFeatureParameterValue max{};
    float step{0.01f};
    bool read_only{false};
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
    virtual std::vector<RenderFeatureInfo> defaultRenderFeatureInfos() const = 0;
    virtual std::vector<RenderFeatureParameterInfo>
        defaultRenderFeatureParameters(std::string_view feature_name) const = 0;
    virtual bool setDefaultRenderFeatureEnabled(std::string_view feature_name, bool enabled) = 0;
    virtual bool setDefaultRenderFeatureParameter(std::string_view feature_name,
                                                  std::string_view parameter_name,
                                                  const RenderFeatureParameterValue& value) = 0;
    virtual std::vector<RenderDebugViewModeInfo> renderDebugViewModes() const = 0;
    virtual RenderDebugViewMode renderDebugViewMode() const noexcept = 0;
    virtual void setRenderDebugViewMode(RenderDebugViewMode mode) = 0;
    virtual float renderDebugVelocityScale() const noexcept = 0;
    virtual void setRenderDebugVelocityScale(float scale) = 0;
    virtual TextureView renderDebugTextureView() const = 0;
    virtual float frameTimeMilliseconds() const noexcept = 0;
    virtual float framesPerSecond() const noexcept = 0;
    virtual UVec2 sceneOutputSize() const noexcept = 0;
};

} // namespace luna::editor
