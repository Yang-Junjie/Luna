#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/Editor/ImporterManager.h"
#include "Core/Log.h"
#include "EditorApi/EditorCommandService.h"
#include "EditorApi/EditorStandardCommands.h"
#include "EditorApi/EditorUi.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "EditorStyle.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditorApp.h"
#include "LunaEditorLayer.h"
#include "Panels/BuiltinMaterialsPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ScriptPluginsPanel.h"
#include "Plugins/AssetLoadingPlugin.h"
#include "Plugins/BackendCapabilitiesPlugin.h"
#include "Plugins/CoreCommandsPlugin.h"
#include "Plugins/EditorApiSamplePlugin.h"
#include "Plugins/RenderDebugPlugin.h"
#include "Plugins/RenderFeaturesPlugin.h"
#include "Plugins/RenderProfilerPlugin.h"
#include "Plugins/SceneSettingsPlugin.h"
#include "Plugins/SceneStatusPlugin.h"
#include "Plugins/ViewportPlugin.h"
#include "Platform/Common/FileDialogs.h"
#include "Project/BuiltinMaterialOverrides.h"
#include "Project/ProjectInfo.h"
#include "Project/ProjectManager.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"
#include "Script/ScriptPluginManager.h"
#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderProfileExporter.h"
#include "Shell/EditorShell.h"

#include <Backend.h>
#include <Instance.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>
#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr const char* kProjectFileFilter = "Luna Project (*.lunaproj)\0*.lunaproj\0";
constexpr const char* kSceneFileFilter = "Luna Scene (*.lunascene)\0*.lunascene\0";
constexpr float kUiScaleChangeThreshold = 0.01f;

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

const char* gizmoOperationToString(luna::GizmoOperation operation)
{
    switch (operation) {
        case luna::GizmoOperation::Translate:
            return "Translate";
        case luna::GizmoOperation::Rotate:
            return "Rotate";
        case luna::GizmoOperation::Scale:
            return "Scale";
    }
    return "Unknown";
}

ImGuizmo::OPERATION toImGuizmoOperation(luna::GizmoOperation operation)
{
    switch (operation) {
        case luna::GizmoOperation::Translate:
            return ImGuizmo::TRANSLATE;
        case luna::GizmoOperation::Rotate:
            return ImGuizmo::ROTATE;
        case luna::GizmoOperation::Scale:
            return ImGuizmo::SCALE;
    }
    return ImGuizmo::TRANSLATE;
}

ImGuizmo::MODE toImGuizmoMode(luna::GizmoMode mode)
{
    return mode == luna::GizmoMode::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

void logEditorAssetSyncStats(const luna::ImporterManager::ImportStats& stats)
{
    LUNA_EDITOR_INFO(
        "Project asset sync: discovered={}, imported_missing={}, loaded_existing={}, rebuilt={}, unsupported={}, "
        "script_skipped_no_plugin={}, script_skipped_unsupported_language={}, failed={}, missing_after_sync={}, "
        "generated_models={}, generated_model_meta={}, generated_materials={}, generated_material_meta={}, "
        "generated_texture_meta={}, generated_model_failures={}",
        stats.discoveredAssets,
        stats.importedMissingAssets,
        stats.loadedExistingMetadata,
        stats.rebuiltMetadata,
        stats.unsupportedFilesSkipped,
        stats.scriptFilesSkippedNoPlugin,
        stats.scriptFilesSkippedUnsupportedLanguage,
        stats.failedAssets,
        stats.missingMetadataAfterSync,
        stats.generatedModelFiles,
        stats.generatedModelMetadata,
        stats.generatedMaterialFiles,
        stats.generatedMaterialMetadata,
        stats.generatedTextureMetadata,
        stats.failedGeneratedModelAssets);
}

std::filesystem::path projectDialogDefaultPath()
{
    if (const auto project_root = luna::ProjectManager::instance().getProjectRootPath()) {
        return *project_root;
    }

    return std::filesystem::current_path();
}

std::optional<std::filesystem::path> makeScenePathRelativeToProject(const std::filesystem::path& scene_file_path)
{
    const auto project_root = luna::ProjectManager::instance().getProjectRootPath();
    if (!project_root || scene_file_path.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(scene_file_path, *project_root, ec);
    if (ec) {
        return std::nullopt;
    }

    relative_path = relative_path.lexically_normal();
    if (relative_path.empty() || relative_path.is_absolute()) {
        return std::nullopt;
    }

    const std::string relative_string = relative_path.generic_string();
    if (relative_string == "." || relative_string.starts_with("..")) {
        return std::nullopt;
    }

    return relative_path;
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

LunaEditorLayer::LunaEditorLayer(LunaEditorApplication& application)
    : Layer("LunaEditorLayer"),
      m_application(&application),
      m_scene_hierarchy_panel(std::make_unique<SceneHierarchyPanel>(*this)),
      m_inspector_panel(std::make_unique<InspectorPanel>(*this)),
      m_builtin_materials_panel(std::make_unique<BuiltinMaterialsPanel>()),
      m_content_browser_panel(std::make_unique<ContentBrowserPanel>(*this)),
      m_script_plugins_panel(std::make_unique<ScriptPluginsPanel>(*this))
{
    m_editor_shell = std::make_unique<editor::EditorShell>(*this);
    m_editor_shell->loadPlugin(editor::createCoreCommandsPlugin());
    m_editor_shell->loadPlugin(editor::createViewportPlugin());
    m_editor_shell->loadPlugin(editor::createSceneStatusPlugin());
    m_editor_shell->loadPlugin(editor::createSceneSettingsPlugin());
    m_editor_shell->loadPlugin(editor::createAssetLoadingPlugin());
    m_editor_shell->loadPlugin(editor::createBackendCapabilitiesPlugin());
    m_editor_shell->loadPlugin(editor::createRenderDebugPlugin());
    m_editor_shell->loadPlugin(editor::createRenderFeaturesPlugin());
    m_editor_shell->loadPlugin(editor::createRenderProfilerPlugin());
    m_editor_shell->loadPlugin(editor::createEditorApiSamplePlugin());
}

LunaEditorLayer::~LunaEditorLayer() = default;

void LunaEditorLayer::onAttach()
{
    if (m_application == nullptr) {
        return;
    }

    m_editor_runtime.scene().setAssetLoadBehavior(Scene::AssetLoadBehavior::NonBlocking);
    createScene();

    if (m_application->getImGuiLayer() != nullptr) {
        m_viewport_session.configureRenderer(m_application->getRenderer(), true);
        syncEditorGridFeatureState();
        syncPickDebugVisualizationState();
    } else {
        m_viewport_session.configureRenderer(m_application->getRenderer(), false);
        syncEditorGridFeatureState();
        LUNA_EDITOR_INFO("ImGui overlay disabled for backend '{}'",
                         luna::RHI::BackendTypeToString(
                             m_application->getRenderer().getCapabilities().backend_type));
    }
}

void LunaEditorLayer::onDetach()
{
    if (m_application == nullptr) {
        return;
    }

    m_editor_camera.releaseMouseCapture();
    m_editor_camera.setInputEnabled(false);
    endRuntimeViewport();
    m_viewport_session.resetRenderer(m_application->getRenderer());
    m_application->getRenderer().setRenderGraphProfilingEnabled(false);
}

void LunaEditorLayer::onUpdate(Timestep dt)
{
    AssetManager::get().updateAsyncLoads();
    processAuthoringEvents();
    if (m_editor_shell) {
        m_editor_shell->update(dt.getSeconds());
    }
    consumePendingScenePick();
    setRuntimeViewportEnabled(m_runtime_viewport_requested);

    const bool allow_editor_camera = !m_runtime_viewport_enabled &&
                                     (m_viewport_focused || m_viewport_hovered || m_editor_camera.isMouseCaptured()) &&
                                     !ImGuizmo::IsUsing();
    m_editor_camera.setInputEnabled(allow_editor_camera);
    if (!m_runtime_viewport_enabled) {
        m_editor_camera.onUpdate(dt);
        activeRenderScene().renderFromEditorCamera(m_editor_camera.getCamera());
    } else {
        m_editor_camera.releaseMouseCapture();
        m_editor_camera.setInputEnabled(false);
        if (m_runtime_scene_runtime) {
            m_runtime_scene_runtime->update(dt);
        }
    }
}

void LunaEditorLayer::onEvent(Event& event)
{
    if (event.m_handled) {
        return;
    }

    if ((m_viewport_focused || m_viewport_hovered || m_editor_camera.isMouseCaptured()) && !ImGuizmo::IsUsing()) {
        m_editor_camera.onEvent(event);
    }
}

void LunaEditorLayer::onImGuiRender()
{
    if (m_application == nullptr) {
        return;
    }

    syncEditorUiScale();
    ImGuizmo::BeginFrame();
    updateEditorShortcuts();

    drawDockSpace();

    m_scene_hierarchy_panel->onImGuiRender();
    m_inspector_panel->onImGuiRender();
    m_builtin_materials_panel->onImGuiRender(m_show_builtin_materials_panel);
    m_content_browser_panel->onImGuiRender();
    m_script_plugins_panel->onImGuiRender(m_show_script_plugins_panel);
    m_viewport_focused = false;
    m_viewport_hovered = false;
    if (m_editor_shell) {
        m_editor_shell->drawWindows();
    }
}

void LunaEditorLayer::syncEditorUiScale()
{
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

    float ui_scale = 1.0f;
    if (main_viewport != nullptr && std::isfinite(main_viewport->DpiScale) && main_viewport->DpiScale > 0.0f) {
        ui_scale = main_viewport->DpiScale;
    }

    if (std::abs(ui_scale - m_editor_ui_scale) <= kUiScaleChangeThreshold) {
        return;
    }

    editor::applyEditorTheme(editor::EditorThemePreset::ModernLightweight, ui_scale);
    m_editor_ui_scale = editor::getEditorUiScale();
}

void LunaEditorLayer::drawDockSpace()
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::Begin("##EditorDockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    const ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(ImGui::GetID("EditorDockSpace"), ImVec2(0.0f, 0.0f), dockspace_flags);

    if (ImGui::BeginMainMenuBar()) {
        onImGuiMenuBar();
        ImGui::EndMainMenuBar();
    }

    ImGui::End();
}

void LunaEditorLayer::onImGuiMenuBar()
{
    const bool project_loaded = hasProjectLoaded();

    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("Open Project")) {
            const std::filesystem::path project_file_path =
                FileDialogs::openFile(kProjectFileFilter, projectDialogDefaultPath().string());
            if (!project_file_path.empty()) {
                openProject(project_file_path);
            }
        }

        if (ImGui::MenuItem("Create New Project")) {
            const std::filesystem::path project_root_path =
                FileDialogs::selectDirectory(projectDialogDefaultPath().string());
            if (!project_root_path.empty()) {
                ProjectInfo project_info{.Name = "New Project",
                                         .Version = "0.1.0",
                                         .Author = "Junjie Yang",
                                         .Description = "A simple Luna project.",
                                         .StartScene = "./Assets/Scenes/Main.lunascene",
                                         .AssetsPath = "./Assets/"};

                if (ProjectManager::instance().createProject(project_root_path, project_info)) {
                    std::error_code ec;
                    if (!project_info.AssetsPath.empty()) {
                        std::filesystem::create_directories(
                            (project_root_path / project_info.AssetsPath).lexically_normal(), ec);
                    }

                    ec.clear();
                    if (!project_info.StartScene.empty()) {
                        const auto scene_directory =
                            (project_root_path / project_info.StartScene).lexically_normal().parent_path();
                        if (!scene_directory.empty()) {
                            std::filesystem::create_directories(scene_directory, ec);
                        }
                    }

                    openProject(project_root_path / (project_info.Name + ".lunaproj"));
                }
            }
        }

        if (ImGui::MenuItem("Sync Assets", nullptr, false, project_loaded)) {
            syncProjectAssets();
        }
        if (ImGui::MenuItem("Refresh Script Plugins", nullptr, false, project_loaded)) {
            refreshProjectScriptPlugins();
        }
        if (m_editor_shell) {
            m_editor_shell->drawMenuItems("Project");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene", project_loaded)) {
        if (ImGui::MenuItem("Create Scene")) {
            createScene();
        }

        if (ImGui::MenuItem("Open Scene")) {
            openScene();
        }

        if (ImGui::MenuItem("Save Scene")) {
            saveScene();
        }
        if (m_editor_shell) {
            m_editor_shell->drawMenuItems("Scene");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (m_editor_shell) {
            m_editor_shell->drawMenuItems("Edit");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Viewport")) {
        if (m_editor_shell) {
            m_editor_shell->drawMenuItems("Viewport");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Builtin Materials", nullptr, &m_show_builtin_materials_panel);
        ImGui::MenuItem("Script Plugins", nullptr, &m_show_script_plugins_panel);
        if (m_editor_shell) {
            ImGui::Separator();
            m_editor_shell->drawWindowMenuItems();
            m_editor_shell->drawMenuItems("Window");
        }
        ImGui::EndMenu();
    }

    if (m_editor_shell) {
        m_editor_shell->drawMenuBarItems({"Project", "Scene", "Edit", "Viewport", "Window"});
    }
}

void LunaEditorLayer::updateEditorShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !io.KeyCtrl || !m_editor_shell) {
        return;
    }

    const bool redo_shortcut = ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
                               (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false));
    if (redo_shortcut) {
        (void) m_editor_shell->commands().execute(editor::commands::kRedo);
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        (void) m_editor_shell->commands().execute(editor::commands::kUndo);
    }
}

void LunaEditorLayer::drawDefaultSceneViewport(editor::Ui& ui)
{
    if (m_application == nullptr) {
        return;
    }

    auto& renderer = m_application->getRenderer();

    m_viewport_focused = ImGui::IsWindowFocused();
    m_viewport_hovered = ImGui::IsWindowHovered();
    updateGizmoShortcuts();

    const editor::Vec2 available = ui.contentRegionAvail();
    const editor::Vec2 framebuffer_scale = ui.windowFramebufferScale();
    const float viewport_scale_x =
        std::isfinite(framebuffer_scale.x) && framebuffer_scale.x > 0.0f ? framebuffer_scale.x : 1.0f;
    const float viewport_scale_y =
        std::isfinite(framebuffer_scale.y) && framebuffer_scale.y > 0.0f ? framebuffer_scale.y : 1.0f;
    const uint32_t viewport_width = static_cast<uint32_t>((std::max) (available.x * viewport_scale_x, 0.0f));
    const uint32_t viewport_height = static_cast<uint32_t>((std::max) (available.y * viewport_scale_y, 0.0f));
    const auto& viewport_state = m_viewport_session.sync(renderer, m_editor_camera, viewport_width, viewport_height);

    const auto& scene_texture = renderer.getSceneOutputTexture();
    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(scene_texture);
    if (texture_id != 0 && available.x > 0.0f && available.y > 0.0f) {
        const bool flip_uv_y = viewport_state.y_flip;
        const ImVec2 uv0(0.0f, flip_uv_y ? 1.0f : 0.0f);
        const ImVec2 uv1(1.0f, flip_uv_y ? 0.0f : 1.0f);

        ImGui::Image(texture_id, ImVec2{available.x, available.y}, uv0, uv1);
        const ImVec2 viewport_min = ImGui::GetItemRectMin();
        const ImVec2 viewport_max = ImGui::GetItemRectMax();
        const ImVec2 viewport_size = ImGui::GetItemRectSize();
        const bool gizmo_active = !m_runtime_viewport_enabled && drawViewportGizmo(viewport_min, viewport_size);
        if (!gizmo_active) {
            if (!m_runtime_viewport_enabled) {
                requestViewportPick(
                    ImGui::GetItemRectMin(),
                    viewport_max,
                    uv0,
                    uv1,
                    scene_texture ? luna::RHI::Extent2D{scene_texture->GetWidth(), scene_texture->GetHeight()}
                                  : luna::RHI::Extent2D{0, 0});
            }
        }
    } else if (available.x > 0.0f && available.y > 0.0f) {
        ImGui::SetCursorPos(editor::scaleEditorUi(16.0f, 16.0f));
        ui.text("Viewport texture will appear after the first rendered frame.");
    }
}

void LunaEditorLayer::updateGizmoShortcuts()
{
    if (!m_viewport_focused || m_editor_camera.isMouseCaptured() || ImGui::GetIO().WantTextInput ||
        !m_editor_runtime.selectedEntityId().isValid()) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        m_gizmo_operation = GizmoOperation::Translate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        m_gizmo_operation = GizmoOperation::Rotate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        m_gizmo_operation = GizmoOperation::Scale;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        m_gizmo_mode = m_gizmo_mode == GizmoMode::Local ? GizmoMode::World : GizmoMode::Local;
    }
}

bool LunaEditorLayer::drawViewportGizmo(const ImVec2& viewport_min, const ImVec2& viewport_size)
{
    Entity selected_entity = getSelectedEntity();
    if (!selected_entity || !selected_entity.isValid() || !selected_entity.hasComponent<TransformComponent>()) {
        if (m_gizmo_transform_transaction_active) {
            (void) m_editor_runtime.commitTransaction();
            m_gizmo_transform_transaction_active = false;
            processAuthoringEvents();
        }
        return false;
    }

    if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
        return false;
    }

    const auto& camera = m_editor_camera.getCamera();
    const float aspect_ratio = viewport_size.x / viewport_size.y;
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix(aspect_ratio);
    projection[1][1] *= -1.0f;
    glm::mat4 transform = m_editor_runtime.scene().entityManager().getWorldSpaceTransformMatrix(selected_entity);

    ImGuizmo::SetOrthographic(camera.getProjectionType() == Camera::ProjectionType::Orthographic);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport_min.x, viewport_min.y, viewport_size.x, viewport_size.y);
    ImGuizmo::PushID(static_cast<int>(static_cast<uint64_t>(selected_entity.getUUID()) & 0x7fffffff));

    const ImGuizmo::MODE mode = m_gizmo_operation == GizmoOperation::Scale ? ImGuizmo::LOCAL : toImGuizmoMode(m_gizmo_mode);
    ImGuizmo::Manipulate(glm::value_ptr(view),
                         glm::value_ptr(projection),
                         toImGuizmoOperation(m_gizmo_operation),
                         mode,
                         glm::value_ptr(transform));

    const bool gizmo_using = ImGuizmo::IsUsing();
    if (gizmo_using) {
        if (!m_gizmo_transform_transaction_active) {
            m_gizmo_transform_transaction_active = m_editor_runtime.beginTransaction("Transform Entity");
        }
        m_editor_runtime.scene().entityManager().setWorldSpaceTransform(selected_entity, transform);
        m_editor_runtime.markSceneDirty();
    } else if (m_gizmo_transform_transaction_active) {
        (void) m_editor_runtime.commitTransaction();
        m_gizmo_transform_transaction_active = false;
        processAuthoringEvents();
    }

    const bool gizmo_active = ImGuizmo::IsOver() || gizmo_using;
    ImGuizmo::PopID();
    return gizmo_active;
}

void LunaEditorLayer::consumePendingScenePick()
{
    if (m_application == nullptr) {
        return;
    }

    const std::optional<uint32_t> picked_id = m_viewport_session.consumeScenePickResult(m_application->getRenderer());
    if (!picked_id.has_value()) {
        return;
    }

    if (*picked_id == 0) {
        setSelectedEntity({});
        return;
    }

    auto& entity_manager = activeRenderScene().entityManager();
    const entt::entity entity_handle = static_cast<entt::entity>(*picked_id - 1u);
    if (!entity_manager.registry().valid(entity_handle)) {
        setSelectedEntity({});
        return;
    }

    setSelectedEntity(Entity(entity_handle, &entity_manager));
}

void LunaEditorLayer::syncPickDebugVisualizationState() const
{
    if (m_application == nullptr) {
        return;
    }

    m_viewport_session.setPickDebugVisualization(m_application->getRenderer(), m_show_pick_debug_visualization);
}

void LunaEditorLayer::syncEditorGridFeatureState() const
{
    if (m_application == nullptr) {
        return;
    }

    m_viewport_session.setEditorGrid(m_application->getRenderer(), m_show_editor_grid, m_runtime_viewport_enabled);
}

void LunaEditorLayer::requestViewportPick(const ImVec2& image_min,
                                          const ImVec2& image_max,
                                          const ImVec2& uv0,
                                          const ImVec2& uv1,
                                          const luna::RHI::Extent2D& texture_extent) const
{
    if (m_application == nullptr || texture_extent.width == 0 || texture_extent.height == 0 ||
        m_editor_camera.isMouseCaptured() || ImGuizmo::IsOver() || ImGuizmo::IsUsing() ||
        !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    const ImVec2 mouse_position = ImGui::GetMousePos();
    const float image_width = image_max.x - image_min.x;
    const float image_height = image_max.y - image_min.y;
    if (image_width <= 0.0f || image_height <= 0.0f) {
        return;
    }

    if (mouse_position.x < image_min.x || mouse_position.x >= image_max.x || mouse_position.y < image_min.y ||
        mouse_position.y >= image_max.y) {
        return;
    }

    const float local_x = std::clamp((mouse_position.x - image_min.x) / image_width, 0.0f, 0.999999f);
    const float local_y = std::clamp((mouse_position.y - image_min.y) / image_height, 0.0f, 0.999999f);

    const float texture_u = std::clamp(uv0.x + (uv1.x - uv0.x) * local_x, 0.0f, 0.999999f);
    const float texture_v = std::clamp(uv0.y + (uv1.y - uv0.y) * local_y, 0.0f, 0.999999f);

    const uint32_t pixel_x = static_cast<uint32_t>(texture_u * static_cast<float>(texture_extent.width));
    const uint32_t color_pixel_y = (std::min) (
        static_cast<uint32_t>(texture_v * static_cast<float>(texture_extent.height)), texture_extent.height - 1);

    (void) m_viewport_session.requestScenePick(
        m_application->getRenderer(), (std::min) (pixel_x, texture_extent.width - 1), color_pixel_y);
}

const std::string& LunaEditorLayer::getAssetLabel() const
{
    return m_asset_label;
}

std::string LunaEditorLayer::getRenderingBackendName() const
{
    if (m_application == nullptr) {
        return "Unknown";
    }

    return luna::RHI::BackendTypeToString(m_application->getRenderer().getCapabilities().backend_type);
}

editor::RenderingBackendCapabilities LunaEditorLayer::getRenderingBackendCapabilities() const
{
    editor::RenderingBackendCapabilities result{};
    if (m_application == nullptr) {
        result.active_backend_name = "Unknown";
        return result;
    }

    const auto& renderer_capabilities = m_application->getRenderer().getCapabilities();
    const luna::RHI::BackendType current_backend = renderer_capabilities.backend_type;
    const std::vector<luna::RHI::BackendType> compiled_backends = luna::RHI::Instance::GetCompiledBackends();
    const std::optional<luna::RHI::BackendType> default_backend = tryGetDefaultBackend();

    result.active_backend_name = luna::RHI::BackendTypeToString(current_backend);
    result.compiled_backend_names = luna::RHI::DescribeBackendTypes(compiled_backends);
    result.compiled_backends.reserve(compiled_backends.size());
    for (const luna::RHI::BackendType backend : compiled_backends) {
        result.compiled_backends.push_back(editor::RenderingBackendEntry{
            .name = luna::RHI::BackendTypeToString(backend),
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

editor::RenderGraphProfileSnapshot LunaEditorLayer::getRenderGraphProfileSnapshot() const
{
    if (m_application == nullptr) {
        return {};
    }

    return toEditorRenderGraphProfile(m_application->getRenderer().getLastRenderGraphProfile());
}

bool LunaEditorLayer::isRenderGraphProfilingEnabled() const noexcept
{
    return m_application != nullptr && m_application->getRenderer().isRenderGraphProfilingEnabled();
}

void LunaEditorLayer::setRenderGraphProfilingEnabled(bool enabled)
{
    if (m_application != nullptr) {
        m_application->getRenderer().setRenderGraphProfilingEnabled(enabled);
    }
}

std::filesystem::path LunaEditorLayer::defaultRenderProfileExportPath(std::string_view backend_name) const
{
    const std::string backend_label =
        backend_name.empty() ? getRenderingBackendName() : std::string(backend_name);
    return luna::makeDefaultRenderProfileExportPath(backend_label);
}

bool LunaEditorLayer::exportRenderGraphProfileChromeTraceJson(
    const editor::RenderGraphProfileSnapshot& profile,
    const std::filesystem::path& output_path,
    std::string* error_message) const
{
    const luna::RenderGraphProfileSnapshot engine_profile = toEngineRenderGraphProfile(profile);
    const RenderProfileExportOptions options{
        .trace_name = "Luna RenderGraph",
        .backend_name = getRenderingBackendName(),
        .frame_index = profile.frame_index,
    };
    return luna::exportRenderGraphProfileChromeTraceJson(engine_profile, output_path, options, error_message);
}

std::vector<editor::RenderFeatureInfo> LunaEditorLayer::getDefaultRenderFeatureInfos() const
{
    if (m_application == nullptr) {
        return {};
    }

    const auto engine_features = m_application->getRenderer().getDefaultRenderFeatureInfos();
    std::vector<editor::RenderFeatureInfo> result;
    result.reserve(engine_features.size());
    for (const auto& feature : engine_features) {
        result.push_back(toEditorRenderFeatureInfo(feature));
    }
    return result;
}

std::vector<editor::RenderFeatureParameterInfo>
LunaEditorLayer::getDefaultRenderFeatureParameters(std::string_view feature_name) const
{
    if (m_application == nullptr) {
        return {};
    }

    const auto engine_parameters = m_application->getRenderer().getDefaultRenderFeatureParameters(feature_name);
    std::vector<editor::RenderFeatureParameterInfo> result;
    result.reserve(engine_parameters.size());
    for (const auto& parameter : engine_parameters) {
        result.push_back(toEditorRenderFeatureParameterInfo(parameter));
    }
    return result;
}

bool LunaEditorLayer::setDefaultRenderFeatureEnabled(std::string_view feature_name, bool enabled)
{
    return m_application != nullptr &&
           m_application->getRenderer().setDefaultRenderFeatureEnabled(feature_name, enabled);
}

bool LunaEditorLayer::setDefaultRenderFeatureParameter(std::string_view feature_name,
                                                       std::string_view parameter_name,
                                                       const editor::RenderFeatureParameterValue& value)
{
    if (m_application == nullptr) {
        return false;
    }

    const render_flow::RenderFeatureParameterValue engine_value = toEngineRenderFeatureParameterValue(value);
    return m_application->getRenderer().setDefaultRenderFeatureParameter(feature_name, parameter_name, engine_value);
}

std::vector<editor::RenderDebugViewModeInfo> LunaEditorLayer::getRenderDebugViewModes() const
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

editor::RenderDebugViewMode LunaEditorLayer::getRenderDebugViewMode() const noexcept
{
    if (m_application == nullptr) {
        return editor::RenderDebugViewMode::None;
    }

    return toEditorRenderDebugViewMode(m_application->getRenderer().getRenderDebugViewMode());
}

void LunaEditorLayer::setRenderDebugViewMode(editor::RenderDebugViewMode mode)
{
    if (m_application != nullptr) {
        m_application->getRenderer().setRenderDebugViewMode(toEngineRenderDebugViewMode(mode));
    }
}

float LunaEditorLayer::getRenderDebugVelocityScale() const noexcept
{
    return m_application != nullptr ? m_application->getRenderer().getRenderDebugVelocityScale() : 0.0f;
}

void LunaEditorLayer::setRenderDebugVelocityScale(float scale)
{
    if (m_application != nullptr) {
        m_application->getRenderer().setRenderDebugVelocityScale(scale);
    }
}

editor::TextureView LunaEditorLayer::getRenderDebugTextureView() const
{
    if (m_application == nullptr) {
        return {};
    }

    const auto& renderer = m_application->getRenderer();
    const auto& debug_texture = renderer.getRenderDebugOutputTexture();
    if (!debug_texture) {
        return {};
    }

    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(debug_texture);
    return editor::TextureView{
        .id = toEditorTextureHandle(texture_id),
        .size = editor::UVec2{.x = debug_texture->GetWidth(), .y = debug_texture->GetHeight()},
        .y_flip = renderer.getCapabilities().conventions.imgui_render_target_requires_uv_y_flip,
    };
}

float LunaEditorLayer::getFrameTimeMilliseconds() const noexcept
{
    return Application::get().getTimestep().getSeconds() * 1000.0f;
}

float LunaEditorLayer::getFramesPerSecond() const noexcept
{
    const float delta_seconds = Application::get().getTimestep().getSeconds();
    return 1.0f / (std::max)(delta_seconds, 0.0001f);
}

uint32_t LunaEditorLayer::getSceneOutputWidth() const noexcept
{
    if (m_application == nullptr) {
        return 0;
    }

    return m_application->getRenderer().getSceneOutputSize().width;
}

uint32_t LunaEditorLayer::getSceneOutputHeight() const noexcept
{
    if (m_application == nullptr) {
        return 0;
    }

    return m_application->getRenderer().getSceneOutputSize().height;
}

size_t LunaEditorLayer::getRuntimeEntityCount() const noexcept
{
    return m_runtime_scene != nullptr ? m_runtime_scene->entityManager().entityCount() : 0u;
}

std::array<float, 3> LunaEditorLayer::getEditorCameraPosition() const noexcept
{
    const glm::vec3 camera_position = m_editor_camera.getCamera().getPosition();
    return {camera_position.x, camera_position.y, camera_position.z};
}

std::string LunaEditorLayer::getGizmoOperationName() const
{
    return gizmoOperationToString(m_gizmo_operation);
}

std::string LunaEditorLayer::getGizmoModeName() const
{
    return m_gizmo_mode == GizmoMode::World ? "World" : "Local";
}

bool LunaEditorLayer::isPickDebugVisualizationEnabled() const noexcept
{
    return m_show_pick_debug_visualization;
}

void LunaEditorLayer::setPickDebugVisualizationEnabled(bool enabled)
{
    if (m_show_pick_debug_visualization == enabled) {
        return;
    }

    m_show_pick_debug_visualization = enabled;
    syncPickDebugVisualizationState();
}

bool LunaEditorLayer::isEditorGridEnabled() const noexcept
{
    return m_show_editor_grid;
}

void LunaEditorLayer::setEditorGridEnabled(bool enabled)
{
    if (m_show_editor_grid == enabled) {
        return;
    }

    m_show_editor_grid = enabled;
    syncEditorGridFeatureState();
}

Scene& LunaEditorLayer::getScene()
{
    return m_editor_runtime.scene();
}

Scene& LunaEditorLayer::getInspectionScene()
{
    return activeRenderScene();
}

bool LunaEditorLayer::isRuntimeViewportEnabled() const noexcept
{
    return m_runtime_viewport_enabled;
}

bool LunaEditorLayer::isRuntimeViewportRequested() const noexcept
{
    return m_runtime_viewport_requested;
}

void LunaEditorLayer::setRuntimeViewportRequested(bool enabled)
{
    m_runtime_viewport_requested = enabled;
}

editor::ViewportPresentation LunaEditorLayer::syncSceneViewport(uint32_t framebuffer_width, uint32_t framebuffer_height)
{
    if (m_application == nullptr) {
        return {};
    }

    const EditorViewportSyncState& state =
        m_viewport_session.sync(m_application->getRenderer(), m_editor_camera, framebuffer_width, framebuffer_height);
    editor::TextureView texture = getSceneTextureView();
    return editor::ViewportPresentation{
        .scene_texture = texture,
        .framebuffer_size = editor::UVec2{.x = state.width, .y = state.height},
        .presentable = state.presentable && texture.valid(),
    };
}

editor::TextureView LunaEditorLayer::getSceneTextureView() const
{
    if (m_application == nullptr) {
        return {};
    }

    const auto& renderer = m_application->getRenderer();
    const auto& scene_texture = renderer.getSceneOutputTexture();
    if (!scene_texture) {
        return {};
    }

    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(scene_texture);
    const EditorViewportSyncState& state = m_viewport_session.state();
    return editor::TextureView{
        .id = toEditorTextureHandle(texture_id),
        .size = editor::UVec2{.x = scene_texture->GetWidth(), .y = scene_texture->GetHeight()},
        .y_flip = state.y_flip,
    };
}

UUID LunaEditorLayer::getSelectedEntityId() const noexcept
{
    return m_editor_runtime.selectedEntityId();
}

Entity LunaEditorLayer::getSelectedEntity()
{
    return m_editor_runtime.selectedEntity(getInspectionScene());
}

void LunaEditorLayer::setSelectedEntity(Entity entity)
{
    m_editor_runtime.setSelectedEntity(entity);
}

void LunaEditorLayer::setSelectedEntityId(UUID entity_id)
{
    m_editor_runtime.setSelectedEntityId(entity_id);
}

void LunaEditorLayer::markSceneDirty()
{
    m_editor_runtime.markSceneDirty();
    processAuthoringEvents();
}

void LunaEditorLayer::patchRuntimeScriptProperty(UUID entity_id, size_t script_index, size_t property_index)
{
    if (!m_runtime_viewport_enabled || !m_runtime_scene || !m_runtime_scene_runtime ||
        !m_runtime_scene_runtime->isRunning()) {
        return;
    }

    Entity runtime_entity = m_runtime_scene->entityManager().findEntityByUUID(entity_id);
    if (!runtime_entity || !runtime_entity.hasComponent<ScriptComponent>()) {
        return;
    }

    const ScriptComponent& runtime_script_component = runtime_entity.getComponent<ScriptComponent>();
    if (script_index >= runtime_script_component.scripts.size()) {
        return;
    }

    const ScriptEntry& runtime_script = runtime_script_component.scripts[script_index];
    if (property_index >= runtime_script.properties.size()) {
        return;
    }

    const ScriptProperty& runtime_property = runtime_script.properties[property_index];
    m_runtime_scene_runtime->setScriptProperty(entity_id, runtime_script.id, runtime_property, property_index);
}

bool LunaEditorLayer::openSceneFile(const std::filesystem::path& scene_file_path)
{
    return openScene(scene_file_path, true);
}

Entity LunaEditorLayer::createEntity(const std::string& name, Entity parent)
{
    Entity entity = m_editor_runtime.createEntity(name, parent);
    if (!entity) {
        return {};
    }

    processAuthoringEvents();
    return entity;
}

Entity LunaEditorLayer::createEntityFromModelAsset(AssetHandle model_handle, Entity parent)
{
    Entity root = m_editor_runtime.createEntityFromModelAsset(model_handle, parent);
    if (!root) {
        return {};
    }

    processAuthoringEvents();
    return root;
}

Entity LunaEditorLayer::createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent)
{
    Entity entity = m_editor_runtime.createEntityFromMeshAsset(mesh_handle, parent);
    if (!entity) {
        return {};
    }

    processAuthoringEvents();
    return entity;
}

Entity LunaEditorLayer::createPrimitiveEntity(AssetHandle mesh_handle, Entity parent)
{
    Entity entity = m_editor_runtime.createPrimitiveEntity(mesh_handle, parent);
    if (!entity) {
        return {};
    }

    processAuthoringEvents();
    return entity;
}

Entity LunaEditorLayer::createCameraEntity(Entity parent)
{
    Entity entity = m_editor_runtime.createCameraEntity(parent);
    if (!entity) {
        return {};
    }

    processAuthoringEvents();
    return entity;
}

Entity LunaEditorLayer::createDirectionalLightEntity(Entity parent)
{
    Entity entity = m_editor_runtime.createDirectionalLightEntity(parent);
    if (!entity) {
        return {};
    }

    processAuthoringEvents();
    return entity;
}

Entity LunaEditorLayer::createPointLightEntity(Entity parent)
{
    Entity entity = m_editor_runtime.createPointLightEntity(parent);
    if (!entity) {
        return {};
    }

    processAuthoringEvents();
    return entity;
}

Entity LunaEditorLayer::createSpotLightEntity(Entity parent)
{
    Entity entity = m_editor_runtime.createSpotLightEntity(parent);
    if (!entity) {
        return {};
    }

    processAuthoringEvents();
    return entity;
}

bool LunaEditorLayer::destroyEntity(Entity entity)
{
    const bool destroyed = m_editor_runtime.destroyEntity(entity);
    if (!destroyed) {
        return false;
    }

    processAuthoringEvents();
    return true;
}

bool LunaEditorLayer::reparentEntity(Entity entity, Entity parent, bool preserve_world_transform)
{
    const bool changed = m_editor_runtime.reparentEntity(entity, parent, preserve_world_transform);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::addComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    const bool changed = m_editor_runtime.addComponent(entity, component_kind);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::removeComponent(Entity entity, authoring::AuthoringComponentKind component_kind)
{
    const bool changed = m_editor_runtime.removeComponent(entity, component_kind);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

void LunaEditorLayer::applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle)
{
    if (m_editor_runtime.applyMeshAssetToEntity(entity, mesh_handle)) {
        processAuthoringEvents();
    }
}

bool LunaEditorLayer::setEntityName(Entity entity, std::string name)
{
    const bool changed = m_editor_runtime.setEntityName(entity, std::move(name));
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::setEntityTransform(Entity entity, const TransformComponent& transform)
{
    const bool changed = m_editor_runtime.setEntityTransform(entity, transform);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::setCameraComponent(Entity entity, const CameraComponent& camera_component)
{
    const bool changed = m_editor_runtime.setCameraComponent(entity, camera_component);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::setLightComponent(Entity entity, const LightComponent& light_component)
{
    const bool changed = m_editor_runtime.setLightComponent(entity, light_component);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::setMeshComponent(Entity entity, const MeshComponent& mesh_component)
{
    const bool changed = m_editor_runtime.setMeshComponent(entity, mesh_component);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::setScriptComponent(Entity entity, const ScriptComponent& script_component)
{
    const bool changed = m_editor_runtime.setScriptComponent(entity, script_component);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings)
{
    const bool changed = m_editor_runtime.setSceneEnvironmentSettings(settings);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

bool LunaEditorLayer::setSceneShadowSettings(const SceneShadowSettings& settings)
{
    const bool changed = m_editor_runtime.setSceneShadowSettings(settings);
    if (changed) {
        processAuthoringEvents();
    }
    return changed;
}

void LunaEditorLayer::openBuiltinMaterialsPanel(AssetHandle material_handle)
{
    if (material_handle.isValid()) {
        m_builtin_materials_panel->focusMaterial(material_handle);
    }
    m_show_builtin_materials_panel = true;
}

bool LunaEditorLayer::hasProjectLoaded() const
{
    return ProjectManager::instance().getProjectRootPath().has_value() &&
           ProjectManager::instance().getProjectInfo().has_value();
}

void LunaEditorLayer::refreshProjectScriptPlugins()
{
    refreshScriptPluginCandidates();

    const auto project_info = ProjectManager::instance().getProjectInfo();
    const ScriptPluginSelectionResult selection =
        ScriptPluginManager::instance().resolveProjectSelection(project_info ? &*project_info : nullptr);

    if (!selection.StatusMessage.empty()) {
        m_script_plugin_status = selection.StatusMessage;
    } else {
        m_script_plugin_status.clear();
    }

    if (selection.isResolved() && selection.Candidate != nullptr) {
        const ProjectInfo& current_project_info = *project_info;
        const std::string selected_plugin_id = selection.Candidate->Manifest.PluginId;
        const std::string selected_backend_name = selection.BackendName;
        if (current_project_info.Scripting.SelectedPluginId != selected_plugin_id ||
            current_project_info.Scripting.SelectedBackendName != selected_backend_name) {
            setProjectScriptPluginSelection(selection.Candidate, false);
        }
    }
}

const std::vector<ScriptPluginCandidate>& LunaEditorLayer::getDiscoveredScriptPlugins() const
{
    return m_script_plugin_candidates;
}

const std::string& LunaEditorLayer::getScriptPluginStatus() const
{
    return m_script_plugin_status;
}

const ScriptPluginCandidate* LunaEditorLayer::getSelectedScriptPluginCandidate() const
{
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_info) {
        return nullptr;
    }

    const ScriptPluginSelectionResult selection =
        ScriptPluginManager::instance().resolveProjectSelection(&*project_info);
    return selection.Candidate;
}

bool LunaEditorLayer::selectScriptPlugin(const ScriptPluginCandidate* candidate)
{
    if (!setProjectScriptPluginSelection(candidate)) {
        return false;
    }

    const auto project_info = ProjectManager::instance().getProjectInfo();
    const ScriptPluginSelectionResult selection =
        ScriptPluginManager::instance().resolveProjectSelection(project_info ? &*project_info : nullptr);
    m_script_plugin_status = selection.StatusMessage;

    return true;
}

void LunaEditorLayer::resetEditorState()
{
    endRuntimeViewport();
    m_runtime_viewport_requested = false;
    m_editor_runtime.resetScene();
    m_asset_label = "No scene loaded";
    m_show_pick_debug_visualization = false;
    syncPickDebugVisualizationState();
    syncEditorGridFeatureState();
}

void LunaEditorLayer::setRuntimeViewportEnabled(bool enabled)
{
    if (enabled == m_runtime_viewport_enabled) {
        return;
    }

    if (m_gizmo_transform_transaction_active) {
        (void) m_editor_runtime.commitTransaction();
        m_gizmo_transform_transaction_active = false;
        processAuthoringEvents();
    }

    if (enabled) {
        beginRuntimeViewport();
    } else {
        endRuntimeViewport();
    }
    syncEditorGridFeatureState();
}

void LunaEditorLayer::beginRuntimeViewport()
{
    const std::string runtime_scene_snapshot = SceneSerializer::serializeToString(m_editor_runtime.scene());
    if (runtime_scene_snapshot.empty()) {
        LUNA_EDITOR_WARN("Failed to serialize current editor scene for runtime viewport");
        m_runtime_viewport_enabled = false;
        m_runtime_viewport_requested = false;
        return;
    }

    m_runtime_scene = std::make_unique<Scene>();
    if (!SceneSerializer::deserializeFromString(*m_runtime_scene, runtime_scene_snapshot, "runtime viewport snapshot")) {
        LUNA_EDITOR_WARN("Failed to create runtime scene snapshot for runtime viewport");
        m_runtime_scene.reset();
        m_runtime_viewport_enabled = false;
        m_runtime_viewport_requested = false;
        return;
    }

    m_runtime_scene->setAssetLoadBehavior(m_editor_runtime.scene().getAssetLoadBehavior());
    m_runtime_scene_runtime = std::make_unique<SceneRuntime>(*m_runtime_scene);
    const auto project_info = ProjectManager::instance().getProjectInfo();
    m_runtime_scene_runtime->setScriptRuntime(
        ScriptPluginManager::instance().createRuntimeForProject(project_info ? &*project_info : nullptr));
    if (!m_runtime_scene_runtime->start()) {
        LUNA_EDITOR_WARN("Failed to start runtime viewport scene");
        m_runtime_scene_runtime.reset();
        m_runtime_scene.reset();
        m_runtime_viewport_enabled = false;
        m_runtime_viewport_requested = false;
        return;
    }

    m_runtime_viewport_enabled = true;
    m_editor_camera.releaseMouseCapture();
    m_editor_camera.setInputEnabled(false);
    LUNA_EDITOR_INFO("Runtime viewport started with {} entities", m_runtime_scene->entityManager().entityCount());
}

void LunaEditorLayer::endRuntimeViewport()
{
    if (!m_runtime_viewport_enabled && !m_runtime_scene && !m_runtime_scene_runtime) {
        return;
    }

    if (m_runtime_scene_runtime) {
        m_runtime_scene_runtime->stop();
        m_runtime_scene_runtime.reset();
    }

    m_runtime_scene.reset();
    m_runtime_viewport_enabled = false;
    LUNA_EDITOR_INFO("Runtime viewport stopped");
}

Scene& LunaEditorLayer::activeRenderScene()
{
    return m_runtime_viewport_enabled && m_runtime_scene ? *m_runtime_scene : m_editor_runtime.scene();
}

void LunaEditorLayer::processAuthoringEvents()
{
    const std::vector<authoring::AuthoringEvent> events = m_editor_runtime.consumeAuthoringEvents();
    if (events.empty()) {
        return;
    }

    bool update_scene_label = false;
    bool validate_selection = false;

    for (const auto& event : events) {
        switch (event.type) {
            case authoring::AuthoringEventType::SceneReset:
            case authoring::AuthoringEventType::SceneCreated:
            case authoring::AuthoringEventType::SceneLoaded:
            case authoring::AuthoringEventType::HistoryChanged:
                update_scene_label = true;
                validate_selection = true;
                break;
            case authoring::AuthoringEventType::SceneSaved:
            case authoring::AuthoringEventType::SceneDirtyChanged:
            case authoring::AuthoringEventType::EntityCreated:
            case authoring::AuthoringEventType::EntityModified:
            case authoring::AuthoringEventType::EntityDestroyed:
            case authoring::AuthoringEventType::EntityReparented:
            case authoring::AuthoringEventType::ComponentAdded:
            case authoring::AuthoringEventType::ComponentRemoved:
                update_scene_label = true;
                validate_selection = true;
                break;
            case authoring::AuthoringEventType::SceneSettingsChanged:
                update_scene_label = true;
                break;
        }
    }

    if (validate_selection) {
        m_editor_runtime.validateSelection();
    }

    if (update_scene_label) {
        updateSceneLabel();
    }
}

void LunaEditorLayer::createScene()
{
    endRuntimeViewport();
    m_runtime_viewport_requested = false;
    m_editor_runtime.clearSelection();
    m_show_pick_debug_visualization = false;
    syncPickDebugVisualizationState();
    syncEditorGridFeatureState();

    const auto bootstrap = m_editor_runtime.createScene();
    if (bootstrap.directional_light) {
        setSelectedEntity(bootstrap.directional_light);
    } else if (bootstrap.camera) {
        setSelectedEntity(bootstrap.camera);
    }
    processAuthoringEvents();
    LUNA_EDITOR_INFO("Created a new scene with a primary camera and directional light");
}

bool LunaEditorLayer::syncProjectAssets()
{
    if (!hasProjectLoaded()) {
        LUNA_EDITOR_WARN("Cannot sync assets because no project is currently loaded");
        return false;
    }

    const ImporterManager::ImportStats stats = ImporterManager::syncProjectAssets();
    logEditorAssetSyncStats(stats);
    m_content_browser_panel->requestRefresh();
    return stats.failedAssets == 0 && stats.missingMetadataAfterSync == 0;
}

bool LunaEditorLayer::openProject(const std::filesystem::path& project_file_path)
{
    if (project_file_path.empty()) {
        return false;
    }

    if (!ProjectManager::instance().loadProject(project_file_path)) {
        LUNA_EDITOR_WARN("Failed to load project '{}'", project_file_path.string());
        return false;
    }

    resetEditorState();

    AssetManager::get().clear();
    AssetDatabase::clear();
    syncProjectAssets();
    AssetManager::get().init();
    BuiltinMaterialOverrides::load();
    refreshProjectScriptPlugins();

    createScene();

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (project_root && project_info && !project_info->StartScene.empty()) {
        const std::filesystem::path start_scene_path =
            SceneSerializer::normalizeScenePath((*project_root / project_info->StartScene).lexically_normal());
        if (std::filesystem::exists(start_scene_path)) {
            if (!openScene(start_scene_path, false)) {
                createScene();
                m_editor_runtime.setSceneFilePath(start_scene_path);
                updateSceneLabel();
            }
        } else {
            m_editor_runtime.setSceneFilePath(start_scene_path);
            updateSceneLabel();
            LUNA_EDITOR_WARN("Configured StartScene '{}' does not exist. Saving will create it at that location.",
                             start_scene_path.string());
        }
    } else {
        updateSceneLabel();
        LUNA_EDITOR_INFO("Project '{}' does not define a StartScene. Using an empty scene.",
                         project_file_path.string());
    }

    LUNA_EDITOR_INFO("Loaded project '{}' with {} scene entities",
                     project_file_path.string(),
                     m_editor_runtime.scene().entityManager().entityCount());
    m_content_browser_panel->requestRefresh();
    return true;
}

bool LunaEditorLayer::openScene()
{
    const std::filesystem::path scene_file_path =
        FileDialogs::openFile(kSceneFileFilter, sceneDialogDefaultPath().string());
    if (scene_file_path.empty()) {
        return false;
    }

    return openScene(scene_file_path, true);
}

bool LunaEditorLayer::openScene(const std::filesystem::path& scene_file_path, bool update_project_start_scene)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    endRuntimeViewport();
    m_runtime_viewport_requested = false;

    if (!m_editor_runtime.openScene(normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to open scene '{}'", normalized_scene_path.string());
        return false;
    }

    processAuthoringEvents();

    if (update_project_start_scene) {
        syncProjectStartScene(normalized_scene_path);
    }

    LUNA_EDITOR_INFO(
        "Opened scene '{}' with {} entities",
        normalized_scene_path.string(),
        m_editor_runtime.scene().entityManager().entityCount());
    return true;
}

bool LunaEditorLayer::saveScene()
{
    if (m_editor_runtime.sceneFilePath().empty()) {
        return saveSceneAs();
    }

    return saveSceneAs(m_editor_runtime.sceneFilePath());
}

bool LunaEditorLayer::saveSceneAs()
{
    const std::filesystem::path scene_file_path =
        FileDialogs::saveFile(kSceneFileFilter, sceneDialogDefaultPath().string());
    if (scene_file_path.empty()) {
        return false;
    }

    return saveSceneAs(scene_file_path);
}

bool LunaEditorLayer::saveSceneAs(const std::filesystem::path& scene_file_path)
{
    const std::filesystem::path normalized_scene_path = SceneSerializer::normalizeScenePath(scene_file_path);
    if (normalized_scene_path.empty()) {
        return false;
    }

    if (!m_editor_runtime.saveSceneAs(normalized_scene_path)) {
        LUNA_EDITOR_WARN("Failed to save scene '{}'", normalized_scene_path.string());
        return false;
    }

    processAuthoringEvents();
    syncProjectStartScene(normalized_scene_path);
    m_content_browser_panel->requestRefresh();

    LUNA_EDITOR_INFO("Saved scene '{}' to '{}'", m_editor_runtime.scene().getName(), normalized_scene_path.string());
    return true;
}

bool LunaEditorLayer::canUndo() const noexcept
{
    return !m_runtime_viewport_enabled && m_editor_runtime.canUndo();
}

bool LunaEditorLayer::canRedo() const noexcept
{
    return !m_runtime_viewport_enabled && m_editor_runtime.canRedo();
}

bool LunaEditorLayer::undoEditorCommand()
{
    return undo();
}

bool LunaEditorLayer::redoEditorCommand()
{
    return redo();
}

bool LunaEditorLayer::undo()
{
    if (m_runtime_viewport_enabled || !m_editor_runtime.undo()) {
        return false;
    }

    processAuthoringEvents();
    return true;
}

bool LunaEditorLayer::redo()
{
    if (m_runtime_viewport_enabled || !m_editor_runtime.redo()) {
        return false;
    }

    processAuthoringEvents();
    return true;
}

std::filesystem::path LunaEditorLayer::sceneDialogDefaultPath() const
{
    if (!m_editor_runtime.sceneFilePath().empty()) {
        const std::filesystem::path parent_path = m_editor_runtime.sceneFilePath().parent_path();
        if (!parent_path.empty() && std::filesystem::exists(parent_path)) {
            return parent_path;
        }
    }

    const auto project_root = ProjectManager::instance().getProjectRootPath();
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (project_root && project_info) {
        const std::filesystem::path scenes_directory =
            (*project_root / project_info->AssetsPath / "Scenes").lexically_normal();
        if (std::filesystem::exists(scenes_directory)) {
            return scenes_directory;
        }

        const std::filesystem::path assets_directory = (*project_root / project_info->AssetsPath).lexically_normal();
        if (std::filesystem::exists(assets_directory)) {
            return assets_directory;
        }

        return *project_root;
    }

    return projectDialogDefaultPath();
}

void LunaEditorLayer::updateSceneLabel()
{
    const char* dirty_suffix = m_editor_runtime.isSceneDirty() ? " *" : "";
    if (!m_editor_runtime.sceneFilePath().empty()) {
        if (const auto relative_path = makeScenePathRelativeToProject(m_editor_runtime.sceneFilePath())) {
            m_asset_label = relative_path->generic_string() + dirty_suffix;
            return;
        }

        m_asset_label = m_editor_runtime.sceneFilePath().lexically_normal().string() + dirty_suffix;
        return;
    }

    const std::string scene_name =
        m_editor_runtime.scene().getName().empty() ? "Untitled" : m_editor_runtime.scene().getName();
    m_asset_label = scene_name + SceneSerializer::FileExtension + std::string(" (unsaved)");
}

void LunaEditorLayer::syncProjectStartScene(const std::filesystem::path& scene_file_path)
{
    const auto relative_scene_path = makeScenePathRelativeToProject(scene_file_path);
    if (!relative_scene_path) {
        LUNA_EDITOR_WARN("Scene '{}' is outside the current project root. StartScene was not updated.",
                         scene_file_path.string());
        return;
    }

    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_info) {
        return;
    }

    if (project_info->StartScene.lexically_normal() == relative_scene_path->lexically_normal()) {
        return;
    }

    ProjectInfo updated_project_info = *project_info;
    updated_project_info.StartScene = *relative_scene_path;
    ProjectManager::instance().setProjectInfo(updated_project_info);

    if (ProjectManager::instance().saveProject()) {
        LUNA_EDITOR_INFO("Updated project StartScene to '{}'", relative_scene_path->generic_string());
    } else {
        LUNA_EDITOR_WARN("Failed to persist updated StartScene '{}'", relative_scene_path->generic_string());
    }
}

void LunaEditorLayer::refreshScriptPluginCandidates()
{
    const auto project_root = ProjectManager::instance().getProjectRootPath();
    ScriptPluginManager::instance().refreshDiscoveredPlugins(project_root);
    m_script_plugin_candidates = ScriptPluginManager::instance().getDiscoveredPlugins();

    if (!project_root) {
        m_script_plugin_status.clear();
        return;
    }

    if (m_script_plugin_candidates.empty()) {
        m_script_plugin_status = "No script plugins discovered.";
    } else if (m_script_plugin_candidates.size() == 1) {
        const auto& candidate = m_script_plugin_candidates.front();
        m_script_plugin_status = "Discovered 1 script plugin: " + candidate.Manifest.DisplayName + ".";
    } else {
        m_script_plugin_status =
            "Discovered " + std::to_string(m_script_plugin_candidates.size()) + " script plugins. Select one.";
    }
}

void LunaEditorLayer::resolveProjectScriptPluginSelection(bool persist_changes)
{
    if (!hasProjectLoaded()) {
        m_script_plugin_candidates.clear();
        m_script_plugin_status.clear();
        return;
    }

    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_info) {
        return;
    }

    const ScriptPluginSelectionResult selection =
        ScriptPluginManager::instance().resolveProjectSelection(&*project_info);
    if (!selection.StatusMessage.empty()) {
        m_script_plugin_status = selection.StatusMessage;
    } else {
        m_script_plugin_status.clear();
    }

    if (persist_changes && selection.isResolved() && selection.Candidate != nullptr) {
        if (project_info->Scripting.SelectedPluginId != selection.Candidate->Manifest.PluginId ||
            project_info->Scripting.SelectedBackendName != selection.BackendName) {
            setProjectScriptPluginSelection(selection.Candidate, false);
        }
    }
}

bool LunaEditorLayer::setProjectScriptPluginSelection(const ScriptPluginCandidate* candidate, bool log_changes)
{
    const auto project_info = ProjectManager::instance().getProjectInfo();
    if (!project_info) {
        return false;
    }

    ProjectInfo updated_project_info = *project_info;
    const std::string selected_plugin_id = candidate != nullptr ? candidate->Manifest.PluginId : std::string{};
    const std::string selected_backend_name = candidate != nullptr ? candidate->Manifest.BackendName : std::string{};

    if (updated_project_info.Scripting.SelectedPluginId == selected_plugin_id &&
        updated_project_info.Scripting.SelectedBackendName == selected_backend_name) {
        return true;
    }

    updated_project_info.Scripting.SelectedPluginId = selected_plugin_id;
    updated_project_info.Scripting.SelectedBackendName = selected_backend_name;
    ProjectManager::instance().setProjectInfo(updated_project_info);

    if (!ProjectManager::instance().saveProject()) {
        if (log_changes) {
            LUNA_EDITOR_WARN("Failed to persist selected script plugin '{}'",
                             candidate != nullptr ? candidate->Manifest.PluginId : std::string("<none>"));
        }
        return false;
    }

    if (log_changes) {
        if (candidate != nullptr) {
            LUNA_EDITOR_INFO("Selected script plugin '{}' ({})",
                             candidate->Manifest.PluginId,
                             candidate->Manifest.BackendName);
        } else {
            LUNA_EDITOR_INFO("Cleared project script plugin selection");
        }
    }

    return true;
}

} // namespace luna
