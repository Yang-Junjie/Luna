#include "Rendering/EditorRenderingController.h"

#include "Core/Application.h"
#include "Imgui/ImGuiContext.h"
#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderProfileExporter.h"
#include "Renderer/Renderer.h"

#include <Backend.h>
#include <Instance.h>

#include <algorithm>
#include <array>
#include <exception>
#include <glm/vec4.hpp>
#include <imgui.h>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

struct RenderDebugModeItem {
    luna::RenderDebugViewMode engine_mode;
    luna::editor::RenderDebugViewMode editor_mode;
    const char* label;
};

constexpr std::array<RenderDebugModeItem, 19> kRenderDebugModes{{
    {luna::RenderDebugViewMode::None, luna::editor::RenderDebugViewMode::None, "None"},
    {luna::RenderDebugViewMode::Velocity, luna::editor::RenderDebugViewMode::Velocity, "Velocity"},
    {luna::RenderDebugViewMode::HistoryValidity,
     luna::editor::RenderDebugViewMode::HistoryValidity,
     "History Validity"},
    {luna::RenderDebugViewMode::ShadowCascades,
     luna::editor::RenderDebugViewMode::ShadowCascades,
     "Shadow Cascades"},
    {luna::RenderDebugViewMode::BaseColor, luna::editor::RenderDebugViewMode::BaseColor, "Base Color"},
    {luna::RenderDebugViewMode::Normal, luna::editor::RenderDebugViewMode::Normal, "Normal"},
    {luna::RenderDebugViewMode::Metallic, luna::editor::RenderDebugViewMode::Metallic, "Metallic"},
    {luna::RenderDebugViewMode::Roughness, luna::editor::RenderDebugViewMode::Roughness, "Roughness"},
    {luna::RenderDebugViewMode::DirectLighting,
     luna::editor::RenderDebugViewMode::DirectLighting,
     "Direct Lighting"},
    {luna::RenderDebugViewMode::SpecularIbl, luna::editor::RenderDebugViewMode::SpecularIbl, "Specular IBL"},
    {luna::RenderDebugViewMode::BloomInput, luna::editor::RenderDebugViewMode::BloomInput, "Bloom Input HDR"},
    {luna::RenderDebugViewMode::BloomPrefilter,
     luna::editor::RenderDebugViewMode::BloomPrefilter,
     "Bloom Prefilter"},
    {luna::RenderDebugViewMode::BloomMip0, luna::editor::RenderDebugViewMode::BloomMip0, "Bloom Mip 0"},
    {luna::RenderDebugViewMode::BloomMip1, luna::editor::RenderDebugViewMode::BloomMip1, "Bloom Mip 1"},
    {luna::RenderDebugViewMode::BloomMip2, luna::editor::RenderDebugViewMode::BloomMip2, "Bloom Mip 2"},
    {luna::RenderDebugViewMode::BloomMip3, luna::editor::RenderDebugViewMode::BloomMip3, "Bloom Mip 3"},
    {luna::RenderDebugViewMode::BloomMip4, luna::editor::RenderDebugViewMode::BloomMip4, "Bloom Mip 4"},
    {luna::RenderDebugViewMode::BloomMip5, luna::editor::RenderDebugViewMode::BloomMip5, "Bloom Mip 5"},
    {luna::RenderDebugViewMode::BloomComposite,
     luna::editor::RenderDebugViewMode::BloomComposite,
     "Bloom Composite HDR"},
}};

std::string toOwnedString(std::string_view value)
{
    return std::string(value.data(), value.size());
}

luna::editor::TextureHandle toEditorTextureHandle(ImTextureID texture_id) noexcept
{
    if constexpr (std::is_pointer_v<ImTextureID>) {
        return reinterpret_cast<luna::editor::TextureHandle>(texture_id);
    } else {
        return static_cast<luna::editor::TextureHandle>(texture_id);
    }
}

std::optional<luna::RHI::BackendType> tryGetDefaultBackend()
{
    try {
        return luna::RHI::Instance::GetDefaultBackend();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string backendStatusText(luna::RHI::BackendType backend,
                              luna::RHI::BackendType current_backend,
                              std::optional<luna::RHI::BackendType> default_backend)
{
    std::string status;
    if (backend == current_backend) {
        status = "Current";
    }
    if (default_backend && backend == *default_backend) {
        if (!status.empty()) {
            status += ", ";
        }
        status += "Default";
    }
    return status.empty() ? "Available" : status;
}

const char* renderGraphPassTypeToString(luna::RenderGraphPassType type)
{
    switch (type) {
        case luna::RenderGraphPassType::Raster:
            return "Raster";
        case luna::RenderGraphPassType::Compute:
            return "Compute";
        default:
            return "Unknown";
    }
}

luna::editor::RenderGraphProfileSnapshot toEditorRenderGraphProfile(
    const luna::RenderGraphProfileSnapshot& profile)
{
    luna::editor::RenderGraphProfileSnapshot result{};
    result.frame_index = profile.FrameIndex;
    result.total_cpu_time_ms = profile.TotalCpuTimeMs;
    result.total_gpu_time_ms = profile.TotalGpuTimeMs;
    result.gpu_timing_supported = profile.GpuTimingSupported;
    result.gpu_timing_pending = profile.GpuTimingPending;
    result.texture_count = profile.TextureCount;
    result.final_barrier_count = profile.FinalBarrierCount;
    result.passes.reserve(profile.Passes.size());
    for (const auto& pass : profile.Passes) {
        result.passes.push_back(luna::editor::RenderGraphPassProfile{
            .name = pass.Name,
            .type = renderGraphPassTypeToString(pass.Type),
            .cpu_time_ms = pass.CpuTimeMs,
            .gpu_time_ms = pass.GpuTimeMs,
            .has_gpu_time = pass.HasGpuTime,
            .framebuffer_width = pass.FramebufferWidth,
            .framebuffer_height = pass.FramebufferHeight,
            .read_texture_count = pass.ReadTextureCount,
            .write_texture_count = pass.WriteTextureCount,
            .color_attachment_count = pass.ColorAttachmentCount,
            .has_depth_attachment = pass.HasDepthAttachment,
            .pre_barrier_count = pass.PreBarrierCount,
        });
    }
    return result;
}

luna::RenderGraphProfileSnapshot toEngineRenderGraphProfile(const luna::editor::RenderGraphProfileSnapshot& profile)
{
    luna::RenderGraphProfileSnapshot result{};
    result.FrameIndex = profile.frame_index;
    result.TotalCpuTimeMs = profile.total_cpu_time_ms;
    result.TotalGpuTimeMs = profile.total_gpu_time_ms;
    result.GpuTimingSupported = profile.gpu_timing_supported;
    result.GpuTimingPending = profile.gpu_timing_pending;
    result.TextureCount = profile.texture_count;
    result.FinalBarrierCount = profile.final_barrier_count;
    result.Passes.reserve(profile.passes.size());
    for (const auto& pass : profile.passes) {
        result.Passes.push_back(luna::RenderGraphPassProfile{
            .Name = pass.name,
            .Type = pass.type == "Compute" ? luna::RenderGraphPassType::Compute : luna::RenderGraphPassType::Raster,
            .CpuTimeMs = pass.cpu_time_ms,
            .GpuTimeMs = pass.gpu_time_ms,
            .HasGpuTime = pass.has_gpu_time,
            .FramebufferWidth = pass.framebuffer_width,
            .FramebufferHeight = pass.framebuffer_height,
            .ReadTextureCount = pass.read_texture_count,
            .WriteTextureCount = pass.write_texture_count,
            .ColorAttachmentCount = pass.color_attachment_count,
            .HasDepthAttachment = pass.has_depth_attachment,
            .PreBarrierCount = pass.pre_barrier_count,
        });
    }
    return result;
}

luna::editor::RenderFeatureGraphResourceKind toEditorRenderFeatureGraphResourceKind(
    luna::render_flow::RenderFeatureGraphResourceKind kind)
{
    switch (kind) {
        case luna::render_flow::RenderFeatureGraphResourceKind::Texture:
            return luna::editor::RenderFeatureGraphResourceKind::Texture;
        case luna::render_flow::RenderFeatureGraphResourceKind::Buffer:
            return luna::editor::RenderFeatureGraphResourceKind::Buffer;
    }
    return luna::editor::RenderFeatureGraphResourceKind::Texture;
}

luna::editor::RenderFeatureGraphResourceFlags toEditorRenderFeatureGraphResourceFlags(
    luna::render_flow::RenderFeatureGraphResourceFlags flags)
{
    using EditorFlags = luna::editor::RenderFeatureGraphResourceFlags;
    using EngineFlags = luna::render_flow::RenderFeatureGraphResourceFlags;

    EditorFlags result = EditorFlags::None;
    if (flags & EngineFlags::Optional) {
        result |= EditorFlags::Optional;
    }
    if (flags & EngineFlags::External) {
        result |= EditorFlags::External;
    }
    return result;
}

luna::editor::RenderFeatureGraphResource toEditorRenderFeatureGraphResource(
    const luna::render_flow::RenderFeatureGraphResource& resource)
{
    return luna::editor::RenderFeatureGraphResource{
        .name = toOwnedString(resource.name),
        .kind = toEditorRenderFeatureGraphResourceKind(resource.kind),
        .flags = toEditorRenderFeatureGraphResourceFlags(resource.flags),
    };
}

luna::editor::RenderPassResourceAccess toEditorRenderPassResourceAccess(
    luna::render_flow::RenderPassResourceAccess access)
{
    switch (access) {
        case luna::render_flow::RenderPassResourceAccess::Read:
            return luna::editor::RenderPassResourceAccess::Read;
        case luna::render_flow::RenderPassResourceAccess::Write:
            return luna::editor::RenderPassResourceAccess::Write;
        case luna::render_flow::RenderPassResourceAccess::ReadWrite:
            return luna::editor::RenderPassResourceAccess::ReadWrite;
    }
    return luna::editor::RenderPassResourceAccess::Read;
}

luna::editor::RenderPassResourceUsage toEditorRenderPassResourceUsage(
    const luna::render_flow::RenderPassResourceUsage& resource)
{
    return luna::editor::RenderPassResourceUsage{
        .name = toOwnedString(resource.name),
        .kind = toEditorRenderFeatureGraphResourceKind(resource.kind),
        .access = toEditorRenderPassResourceAccess(resource.access),
        .flags = toEditorRenderFeatureGraphResourceFlags(resource.flags),
    };
}

std::vector<luna::editor::RenderFeatureGraphResource> toEditorRenderFeatureGraphResources(
    const std::vector<luna::render_flow::RenderFeatureGraphResource>& resources)
{
    std::vector<luna::editor::RenderFeatureGraphResource> result;
    result.reserve(resources.size());
    for (const auto& resource : resources) {
        result.push_back(toEditorRenderFeatureGraphResource(resource));
    }
    return result;
}

std::vector<luna::editor::RenderPassResourceUsage> toEditorRenderPassResourceUsages(
    const std::vector<luna::render_flow::RenderPassResourceUsage>& resources)
{
    std::vector<luna::editor::RenderPassResourceUsage> result;
    result.reserve(resources.size());
    for (const auto& resource : resources) {
        result.push_back(toEditorRenderPassResourceUsage(resource));
    }
    return result;
}

luna::editor::RenderFeaturePassInfo toEditorRenderFeaturePassInfo(
    const luna::render_flow::RenderFeaturePassInfo& pass)
{
    return luna::editor::RenderFeaturePassInfo{
        .name = pass.name,
        .resources = toEditorRenderPassResourceUsages(pass.resources),
    };
}

std::vector<luna::editor::RenderFeaturePassInfo> toEditorRenderFeaturePassInfos(
    const std::vector<luna::render_flow::RenderFeaturePassInfo>& passes)
{
    std::vector<luna::editor::RenderFeaturePassInfo> result;
    result.reserve(passes.size());
    for (const auto& pass : passes) {
        result.push_back(toEditorRenderFeaturePassInfo(pass));
    }
    return result;
}

luna::editor::RenderFeatureStatusEntry toEditorRenderFeatureStatusEntry(
    const luna::render_flow::RenderFeatureStatusEntry& entry)
{
    return luna::editor::RenderFeatureStatusEntry{
        .name = entry.name,
        .ready = entry.ready,
    };
}

std::vector<luna::editor::RenderFeatureStatusEntry> toEditorRenderFeatureStatusEntries(
    const std::vector<luna::render_flow::RenderFeatureStatusEntry>& entries)
{
    std::vector<luna::editor::RenderFeatureStatusEntry> result;
    result.reserve(entries.size());
    for (const auto& entry : entries) {
        result.push_back(toEditorRenderFeatureStatusEntry(entry));
    }
    return result;
}

luna::editor::RenderFeatureRuntimeStatType toEditorRenderFeatureRuntimeStatType(
    luna::render_flow::RenderFeatureRuntimeStatType type)
{
    switch (type) {
        case luna::render_flow::RenderFeatureRuntimeStatType::UnsignedInteger:
            return luna::editor::RenderFeatureRuntimeStatType::UnsignedInteger;
        case luna::render_flow::RenderFeatureRuntimeStatType::Float:
            return luna::editor::RenderFeatureRuntimeStatType::Float;
        case luna::render_flow::RenderFeatureRuntimeStatType::Bool:
            return luna::editor::RenderFeatureRuntimeStatType::Bool;
    }
    return luna::editor::RenderFeatureRuntimeStatType::UnsignedInteger;
}

luna::editor::RenderFeatureRuntimeStat toEditorRenderFeatureRuntimeStat(
    const luna::render_flow::RenderFeatureRuntimeStat& stat)
{
    return luna::editor::RenderFeatureRuntimeStat{
        .name = stat.name,
        .type = toEditorRenderFeatureRuntimeStatType(stat.type),
        .uint_value = stat.uint_value,
        .float_value = stat.float_value,
        .bool_value = stat.bool_value,
    };
}

std::vector<luna::editor::RenderFeatureRuntimeStat> toEditorRenderFeatureRuntimeStats(
    const std::vector<luna::render_flow::RenderFeatureRuntimeStat>& stats)
{
    std::vector<luna::editor::RenderFeatureRuntimeStat> result;
    result.reserve(stats.size());
    for (const auto& stat : stats) {
        result.push_back(toEditorRenderFeatureRuntimeStat(stat));
    }
    return result;
}

luna::editor::RenderFeatureDiagnostics toEditorRenderFeatureDiagnostics(
    const luna::render_flow::RenderFeatureDiagnostics& diagnostics)
{
    return luna::editor::RenderFeatureDiagnostics{
        .binding_contract_valid = diagnostics.binding_contract_valid,
        .binding_contract_summary = diagnostics.binding_contract_summary,
        .pipeline_resources_valid = diagnostics.pipeline_resources_valid,
        .pipeline_resources_summary = diagnostics.pipeline_resources_summary,
        .pipeline_resources = toEditorRenderFeatureStatusEntries(diagnostics.pipeline_resources),
        .persistent_resources_valid = diagnostics.persistent_resources_valid,
        .persistent_resources_summary = diagnostics.persistent_resources_summary,
        .persistent_resources = toEditorRenderFeatureStatusEntries(diagnostics.persistent_resources),
        .history_resources_valid = diagnostics.history_resources_valid,
        .history_resources_summary = diagnostics.history_resources_summary,
        .history_resources = toEditorRenderFeatureStatusEntries(diagnostics.history_resources),
        .runtime_stats = toEditorRenderFeatureRuntimeStats(diagnostics.runtime_stats),
    };
}

luna::editor::RenderFeatureInfo toEditorRenderFeatureInfo(const luna::render_flow::RenderFeatureInfo& feature)
{
    return luna::editor::RenderFeatureInfo{
        .name = toOwnedString(feature.name),
        .display_name = toOwnedString(feature.display_name),
        .category = toOwnedString(feature.category),
        .enabled = feature.enabled,
        .runtime_toggleable = feature.runtime_toggleable,
        .supported = feature.supported,
        .active = feature.active,
        .support_summary = feature.support_summary,
        .graph_contract_valid = feature.graph_contract_valid,
        .graph_contract_summary = feature.graph_contract_summary,
        .pass_contract_valid = feature.pass_contract_valid,
        .pass_contract_summary = feature.pass_contract_summary,
        .graph_inputs = toEditorRenderFeatureGraphResources(feature.graph_inputs),
        .graph_outputs = toEditorRenderFeatureGraphResources(feature.graph_outputs),
        .passes = toEditorRenderFeaturePassInfos(feature.passes),
        .diagnostics = toEditorRenderFeatureDiagnostics(feature.diagnostics),
    };
}

luna::editor::RenderFeatureParameterType toEditorRenderFeatureParameterType(
    luna::render_flow::RenderFeatureParameterType type)
{
    switch (type) {
        case luna::render_flow::RenderFeatureParameterType::Bool:
            return luna::editor::RenderFeatureParameterType::Bool;
        case luna::render_flow::RenderFeatureParameterType::Int:
            return luna::editor::RenderFeatureParameterType::Int;
        case luna::render_flow::RenderFeatureParameterType::Float:
            return luna::editor::RenderFeatureParameterType::Float;
        case luna::render_flow::RenderFeatureParameterType::Color:
            return luna::editor::RenderFeatureParameterType::Color;
    }
    return luna::editor::RenderFeatureParameterType::Float;
}

luna::render_flow::RenderFeatureParameterType toEngineRenderFeatureParameterType(
    luna::editor::RenderFeatureParameterType type)
{
    switch (type) {
        case luna::editor::RenderFeatureParameterType::Bool:
            return luna::render_flow::RenderFeatureParameterType::Bool;
        case luna::editor::RenderFeatureParameterType::Int:
            return luna::render_flow::RenderFeatureParameterType::Int;
        case luna::editor::RenderFeatureParameterType::Float:
            return luna::render_flow::RenderFeatureParameterType::Float;
        case luna::editor::RenderFeatureParameterType::Color:
            return luna::render_flow::RenderFeatureParameterType::Color;
    }
    return luna::render_flow::RenderFeatureParameterType::Float;
}

luna::editor::RenderFeatureParameterValue toEditorRenderFeatureParameterValue(
    const luna::render_flow::RenderFeatureParameterValue& value)
{
    return luna::editor::RenderFeatureParameterValue{
        .type = toEditorRenderFeatureParameterType(value.type),
        .bool_value = value.bool_value,
        .int_value = value.int_value,
        .float_value = value.float_value,
        .color_value =
            luna::editor::Vec4{.x = value.color_value.x,
                               .y = value.color_value.y,
                               .z = value.color_value.z,
                               .w = value.color_value.w},
    };
}

luna::render_flow::RenderFeatureParameterValue toEngineRenderFeatureParameterValue(
    const luna::editor::RenderFeatureParameterValue& value)
{
    luna::render_flow::RenderFeatureParameterValue result{};
    result.type = toEngineRenderFeatureParameterType(value.type);
    result.bool_value = value.bool_value;
    result.int_value = value.int_value;
    result.float_value = value.float_value;
    result.color_value =
        glm::vec4{value.color_value.x, value.color_value.y, value.color_value.z, value.color_value.w};
    return result;
}

luna::editor::RenderFeatureParameterInfo toEditorRenderFeatureParameterInfo(
    const luna::render_flow::RenderFeatureParameterInfo& parameter)
{
    return luna::editor::RenderFeatureParameterInfo{
        .name = toOwnedString(parameter.name),
        .display_name = toOwnedString(parameter.display_name),
        .type = toEditorRenderFeatureParameterType(parameter.type),
        .value = toEditorRenderFeatureParameterValue(parameter.value),
        .min = toEditorRenderFeatureParameterValue(parameter.min),
        .max = toEditorRenderFeatureParameterValue(parameter.max),
        .step = parameter.step,
        .read_only = parameter.read_only,
    };
}

luna::editor::RenderDebugViewMode toEditorRenderDebugViewMode(luna::RenderDebugViewMode mode) noexcept
{
    for (const RenderDebugModeItem& item : kRenderDebugModes) {
        if (item.engine_mode == mode) {
            return item.editor_mode;
        }
    }
    return luna::editor::RenderDebugViewMode::None;
}

luna::RenderDebugViewMode toEngineRenderDebugViewMode(luna::editor::RenderDebugViewMode mode) noexcept
{
    for (const RenderDebugModeItem& item : kRenderDebugModes) {
        if (item.editor_mode == mode) {
            return item.engine_mode;
        }
    }
    return luna::RenderDebugViewMode::None;
}

} // namespace

namespace luna {

EditorRenderingController::EditorRenderingController(Renderer& renderer) noexcept
    : m_renderer(&renderer)
{}

void EditorRenderingController::bindRenderer(Renderer* renderer) noexcept
{
    m_renderer = renderer;
}

std::string EditorRenderingController::backendName() const
{
    if (m_renderer == nullptr) {
        return "Unknown";
    }

    return RHI::BackendTypeToString(m_renderer->getCapabilities().backend_type);
}

editor::RenderingBackendCapabilities EditorRenderingController::backendCapabilities() const
{
    editor::RenderingBackendCapabilities result{};
    if (m_renderer == nullptr) {
        result.active_backend_name = "Unknown";
        return result;
    }

    const auto& renderer_capabilities = m_renderer->getCapabilities();
    const RHI::BackendType current_backend = renderer_capabilities.backend_type;
    const std::vector<RHI::BackendType> compiled_backends = RHI::Instance::GetCompiledBackends();
    const std::optional<RHI::BackendType> default_backend = tryGetDefaultBackend();

    result.active_backend_name = RHI::BackendTypeToString(current_backend);
    result.compiled_backend_names = RHI::DescribeBackendTypes(compiled_backends);
    result.compiled_backends.reserve(compiled_backends.size());
    for (const RHI::BackendType backend : compiled_backends) {
        result.compiled_backends.push_back(editor::RenderingBackendEntry{
            .name = RHI::BackendTypeToString(backend),
            .status = backendStatusText(backend, current_backend, default_backend),
            .current = backend == current_backend,
            .default_backend = default_backend && backend == *default_backend,
        });
    }

    result.supports_default_render_flow = renderer_capabilities.supports_default_render_flow;
    result.supports_imgui = renderer_capabilities.supports_imgui;
    result.supports_scene_pick_readback = renderer_capabilities.supports_scene_pick_readback;
    result.supports_gpu_timestamp = renderer_capabilities.supports_gpu_timestamp;
    result.gpu_timestamp_uses_disjoint_query = renderer_capabilities.gpu_timestamp_uses_disjoint_query;
    result.supports_graphics_pipeline = renderer_capabilities.supports_graphics_pipeline;
    result.supports_compute_pipeline = renderer_capabilities.supports_compute_pipeline;
    result.supports_sampled_texture = renderer_capabilities.supports_sampled_texture;
    result.supports_storage_texture = renderer_capabilities.supports_storage_texture;
    result.supports_color_attachment = renderer_capabilities.supports_color_attachment;
    result.supports_depth_attachment = renderer_capabilities.supports_depth_attachment;
    result.supports_uniform_buffer = renderer_capabilities.supports_uniform_buffer;
    result.supports_storage_buffer = renderer_capabilities.supports_storage_buffer;
    result.supports_sampler = renderer_capabilities.supports_sampler;

    result.conventions.requires_projection_y_flip =
        renderer_capabilities.conventions.requires_projection_y_flip;
    result.conventions.imgui_clip_top_y_is_negative_one =
        renderer_capabilities.conventions.imgui_clip_top_y_is_negative_one;
    result.conventions.imgui_render_target_requires_uv_y_flip =
        renderer_capabilities.conventions.imgui_render_target_requires_uv_y_flip;
    result.conventions.scene_pick_y_matches_display_y =
        renderer_capabilities.conventions.scene_pick_y_matches_display_y;

    return result;
}

editor::RenderGraphProfileSnapshot EditorRenderingController::renderGraphProfile() const
{
    if (m_renderer == nullptr) {
        return {};
    }

    return toEditorRenderGraphProfile(m_renderer->getLastRenderGraphProfile());
}

bool EditorRenderingController::isRenderGraphProfilingEnabled() const noexcept
{
    return m_renderer != nullptr && m_renderer->isRenderGraphProfilingEnabled();
}

void EditorRenderingController::setRenderGraphProfilingEnabled(bool enabled)
{
    if (m_renderer != nullptr) {
        m_renderer->setRenderGraphProfilingEnabled(enabled);
    }
}

std::filesystem::path EditorRenderingController::defaultRenderProfileExportPath(std::string_view backend_name) const
{
    const std::string backend_label = backend_name.empty() ? backendName() : std::string(backend_name);
    return makeDefaultRenderProfileExportPath(backend_label);
}

bool EditorRenderingController::exportRenderGraphProfileChromeTraceJson(
    const editor::RenderGraphProfileSnapshot& profile,
    const std::filesystem::path& output_path,
    std::string* error_message) const
{
    const RenderGraphProfileSnapshot engine_profile = toEngineRenderGraphProfile(profile);
    const RenderProfileExportOptions options{
        .trace_name = "Luna RenderGraph",
        .backend_name = backendName(),
        .frame_index = profile.frame_index,
    };
    return luna::exportRenderGraphProfileChromeTraceJson(engine_profile, output_path, options, error_message);
}

std::vector<editor::RenderFeatureInfo> EditorRenderingController::defaultRenderFeatureInfos() const
{
    if (m_renderer == nullptr) {
        return {};
    }

    const auto engine_features = m_renderer->getDefaultRenderFeatureInfos();
    std::vector<editor::RenderFeatureInfo> result;
    result.reserve(engine_features.size());
    for (const auto& feature : engine_features) {
        result.push_back(toEditorRenderFeatureInfo(feature));
    }
    return result;
}

std::vector<editor::RenderFeatureParameterInfo>
EditorRenderingController::defaultRenderFeatureParameters(std::string_view feature_name) const
{
    if (m_renderer == nullptr) {
        return {};
    }

    const auto engine_parameters = m_renderer->getDefaultRenderFeatureParameters(feature_name);
    std::vector<editor::RenderFeatureParameterInfo> result;
    result.reserve(engine_parameters.size());
    for (const auto& parameter : engine_parameters) {
        result.push_back(toEditorRenderFeatureParameterInfo(parameter));
    }
    return result;
}

bool EditorRenderingController::setDefaultRenderFeatureEnabled(std::string_view feature_name, bool enabled)
{
    return m_renderer != nullptr && m_renderer->setDefaultRenderFeatureEnabled(feature_name, enabled);
}

bool EditorRenderingController::setDefaultRenderFeatureParameter(std::string_view feature_name,
                                                                 std::string_view parameter_name,
                                                                 const editor::RenderFeatureParameterValue& value)
{
    if (m_renderer == nullptr) {
        return false;
    }

    const render_flow::RenderFeatureParameterValue engine_value = toEngineRenderFeatureParameterValue(value);
    return m_renderer->setDefaultRenderFeatureParameter(feature_name, parameter_name, engine_value);
}

std::vector<editor::RenderDebugViewModeInfo> EditorRenderingController::renderDebugViewModes() const
{
    std::vector<editor::RenderDebugViewModeInfo> result;
    result.reserve(kRenderDebugModes.size());
    for (const RenderDebugModeItem& item : kRenderDebugModes) {
        result.push_back(editor::RenderDebugViewModeInfo{
            .mode = item.editor_mode,
            .label = item.label,
        });
    }
    return result;
}

editor::RenderDebugViewMode EditorRenderingController::renderDebugViewMode() const noexcept
{
    if (m_renderer == nullptr) {
        return editor::RenderDebugViewMode::None;
    }

    return toEditorRenderDebugViewMode(m_renderer->getRenderDebugViewMode());
}

void EditorRenderingController::setRenderDebugViewMode(editor::RenderDebugViewMode mode)
{
    if (m_renderer != nullptr) {
        m_renderer->setRenderDebugViewMode(toEngineRenderDebugViewMode(mode));
    }
}

float EditorRenderingController::renderDebugVelocityScale() const noexcept
{
    return m_renderer != nullptr ? m_renderer->getRenderDebugVelocityScale() : 0.0f;
}

void EditorRenderingController::setRenderDebugVelocityScale(float scale)
{
    if (m_renderer != nullptr) {
        m_renderer->setRenderDebugVelocityScale(scale);
    }
}

editor::TextureView EditorRenderingController::renderDebugTextureView() const
{
    if (m_renderer == nullptr) {
        return {};
    }

    const auto& debug_texture = m_renderer->getRenderDebugOutputTexture();
    if (!debug_texture) {
        return {};
    }

    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(debug_texture);
    return editor::TextureView{
        .id = toEditorTextureHandle(texture_id),
        .size = editor::UVec2{.x = debug_texture->GetWidth(), .y = debug_texture->GetHeight()},
        .y_flip = m_renderer->getCapabilities().conventions.imgui_render_target_requires_uv_y_flip,
    };
}

float EditorRenderingController::frameTimeMilliseconds() const noexcept
{
    return Application::get().getTimestep().getSeconds() * 1000.0f;
}

float EditorRenderingController::framesPerSecond() const noexcept
{
    const float delta_seconds = Application::get().getTimestep().getSeconds();
    return 1.0f / (std::max)(delta_seconds, 0.0001f);
}

editor::UVec2 EditorRenderingController::sceneOutputSize() const noexcept
{
    if (m_renderer == nullptr) {
        return {};
    }

    const RHI::Extent2D size = m_renderer->getSceneOutputSize();
    return editor::UVec2{.x = size.width, .y = size.height};
}

} // namespace luna
