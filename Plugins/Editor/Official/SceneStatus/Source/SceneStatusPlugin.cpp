#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"
#include "SceneStatusPlugin.h"

#include <cstdint>

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.scene-status";
constexpr const char* kWindowId = "luna.editor.scene-status.window";
constexpr std::string_view kDefaultSceneFeatureName = "DefaultScene";
constexpr std::string_view kShowVisibleBoundsParameter = "showVisibleBounds";
constexpr std::string_view kShowCulledBoundsParameter = "showCulledBounds";
constexpr std::string_view kShowCullingFrustumParameter = "showCullingFrustum";
constexpr std::string_view kFreezeCullingCameraParameter = "freezeCullingCamera";

struct SceneVisibilityStats {
    uint64_t submitted{0};
    uint64_t camera_visible{0};
    uint64_t camera_culled{0};
    uint64_t invalid_bounds{0};
    uint64_t shadow_unculled{0};
    uint64_t phase_depth_only{0};
    uint64_t phase_gbuffer{0};
    uint64_t phase_forward_opaque{0};
    uint64_t phase_transparent{0};
    uint64_t phase_shadow_caster{0};
    uint64_t phase_picking{0};
    uint64_t debug_captured{0};
    uint64_t debug_frustums{0};
    uint64_t overlay_vertices{0};
};

struct VisibilityDebugControls {
    bool show_visible_bounds{false};
    bool show_culled_bounds{false};
    bool show_culling_frustum{false};
    bool freeze_culling_camera{false};
    bool has_show_visible_bounds{false};
    bool has_show_culled_bounds{false};
    bool has_show_culling_frustum{false};
    bool has_freeze_culling_camera{false};
};

std::string formatFloat(float value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string formatVec3(const luna::editor::Vec3& value, int precision)
{
    return formatFloat(value.x, precision) + ", " + formatFloat(value.y, precision) + ", " +
           formatFloat(value.z, precision);
}

std::string formatUInt64(uint64_t value)
{
    return std::to_string(value);
}

void assignBoolParameter(VisibilityDebugControls& controls, const luna::editor::RenderFeatureParameterInfo& parameter)
{
    if (parameter.value.type != luna::editor::RenderFeatureParameterType::Bool) {
        return;
    }

    const std::string_view name(parameter.name);
    if (name == kShowVisibleBoundsParameter) {
        controls.show_visible_bounds = parameter.value.bool_value;
        controls.has_show_visible_bounds = true;
    } else if (name == kShowCulledBoundsParameter) {
        controls.show_culled_bounds = parameter.value.bool_value;
        controls.has_show_culled_bounds = true;
    } else if (name == kShowCullingFrustumParameter) {
        controls.show_culling_frustum = parameter.value.bool_value;
        controls.has_show_culling_frustum = true;
    } else if (name == kFreezeCullingCameraParameter) {
        controls.freeze_culling_camera = parameter.value.bool_value;
        controls.has_freeze_culling_camera = true;
    }
}

VisibilityDebugControls visibilityDebugControls(luna::editor::Host& host)
{
    VisibilityDebugControls controls{};
    const std::vector<luna::editor::RenderFeatureParameterInfo> parameters =
        host.rendering().defaultRenderFeatureParameters(kDefaultSceneFeatureName);
    for (const luna::editor::RenderFeatureParameterInfo& parameter : parameters) {
        assignBoolParameter(controls, parameter);
    }
    return controls;
}

void setVisibilityDebugBool(luna::editor::Host& host, std::string_view parameter_name, bool value)
{
    luna::editor::RenderFeatureParameterValue parameter_value{};
    parameter_value.type = luna::editor::RenderFeatureParameterType::Bool;
    parameter_value.bool_value = value;
    (void) host.rendering().setDefaultRenderFeatureParameter(kDefaultSceneFeatureName, parameter_name, parameter_value);
}

void assignVisibilityStat(SceneVisibilityStats& stats, const luna::editor::RenderFeatureRuntimeStat& stat)
{
    if (stat.type != luna::editor::RenderFeatureRuntimeStatType::UnsignedInteger) {
        return;
    }

    const std::string_view name(stat.name);
    if (name == "draws.submitted") {
        stats.submitted = stat.uint_value;
    } else if (name == "draws.camera_visible") {
        stats.camera_visible = stat.uint_value;
    } else if (name == "draws.camera_culled") {
        stats.camera_culled = stat.uint_value;
    } else if (name == "draws.invalid_bounds") {
        stats.invalid_bounds = stat.uint_value;
    } else if (name == "draws.shadow_unculled") {
        stats.shadow_unculled = stat.uint_value;
    } else if (name == "draws.phase.depth_only") {
        stats.phase_depth_only = stat.uint_value;
    } else if (name == "draws.phase.gbuffer") {
        stats.phase_gbuffer = stat.uint_value;
    } else if (name == "draws.phase.forward_opaque") {
        stats.phase_forward_opaque = stat.uint_value;
    } else if (name == "draws.phase.transparent") {
        stats.phase_transparent = stat.uint_value;
    } else if (name == "draws.phase.shadow_caster") {
        stats.phase_shadow_caster = stat.uint_value;
    } else if (name == "draws.phase.picking") {
        stats.phase_picking = stat.uint_value;
    } else if (name == "visibility_debug.captured") {
        stats.debug_captured = stat.uint_value;
    } else if (name == "visibility_debug.culling_frustums") {
        stats.debug_frustums = stat.uint_value;
    } else if (name == "visibility_overlay.vertices") {
        stats.overlay_vertices = stat.uint_value;
    }
}

std::optional<SceneVisibilityStats>
    findSceneVisibilityStats(const std::vector<luna::editor::RenderFeatureInfo>& features)
{
    for (const luna::editor::RenderFeatureInfo& feature : features) {
        if (std::string_view(feature.name) != kDefaultSceneFeatureName) {
            continue;
        }

        SceneVisibilityStats stats{};
        for (const luna::editor::RenderFeatureRuntimeStat& stat : feature.diagnostics.runtime_stats) {
            assignVisibilityStat(stats, stat);
        }
        return stats;
    }
    return std::nullopt;
}

void drawOptionalVisibilityCheckbox(luna::editor::Host& host,
                                    luna::editor::Ui& ui,
                                    std::string_view label,
                                    std::string_view parameter_name,
                                    bool& value,
                                    bool available)
{
    if (!available) {
        return;
    }
    if (ui.checkbox(label, value)) {
        setVisibilityDebugBool(host, parameter_name, value);
    }
}

void drawVisibilityStatsPanel(luna::editor::Host& host, luna::editor::Ui& ui, const SceneVisibilityStats& stats)
{
    VisibilityDebugControls controls = visibilityDebugControls(host);

    ui.heading("Visibility / Culling");
    ui.beginPanel("##SceneStatusVisibilityPanel");
    ui.keyValue("Submitted", formatUInt64(stats.submitted));
    ui.keyValue("Camera Visible", formatUInt64(stats.camera_visible));
    ui.keyValue("Camera Culled", formatUInt64(stats.camera_culled));
    ui.keyValue("Invalid Bounds", formatUInt64(stats.invalid_bounds));
    ui.keyValue("Shadow Unculled", formatUInt64(stats.shadow_unculled));
    ui.separator();
    ui.keyValue("GBuffer Draws", formatUInt64(stats.phase_gbuffer));
    ui.keyValue("Transparent Draws", formatUInt64(stats.phase_transparent));
    ui.keyValue("Shadow Draws", formatUInt64(stats.phase_shadow_caster));
    ui.keyValue("Depth Draws", formatUInt64(stats.phase_depth_only));
    ui.keyValue("Picking Draws", formatUInt64(stats.phase_picking));
    if (stats.phase_forward_opaque > 0) {
        ui.keyValue("Forward Draws", formatUInt64(stats.phase_forward_opaque));
    }
    ui.separator();
    ui.keyValue("Debug Bounds", formatUInt64(stats.debug_captured));
    ui.keyValue("Debug Frustums", formatUInt64(stats.debug_frustums));
    ui.keyValue("Overlay Vertices", formatUInt64(stats.overlay_vertices));
    ui.separator();
    drawOptionalVisibilityCheckbox(host,
                                   ui,
                                   "Show Visible Bounds",
                                   kShowVisibleBoundsParameter,
                                   controls.show_visible_bounds,
                                   controls.has_show_visible_bounds);
    drawOptionalVisibilityCheckbox(host,
                                   ui,
                                   "Show Culled Bounds",
                                   kShowCulledBoundsParameter,
                                   controls.show_culled_bounds,
                                   controls.has_show_culled_bounds);
    drawOptionalVisibilityCheckbox(host,
                                   ui,
                                   "Show Culling Frustum",
                                   kShowCullingFrustumParameter,
                                   controls.show_culling_frustum,
                                   controls.has_show_culling_frustum);
    drawOptionalVisibilityCheckbox(host,
                                   ui,
                                   "Freeze Culling Camera",
                                   kFreezeCullingCameraParameter,
                                   controls.freeze_culling_camera,
                                   controls.has_freeze_culling_camera);
    ui.endPanel();
}

class SceneStatusPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Scene Status",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Scene",
            .default_open = true,
            .draw =
                [](luna::editor::WindowDrawContext& context) {
                    luna::editor::Host& host = context.host();
                    luna::editor::Ui& ui = context.ui();

                    const luna::editor::UVec2 viewport_size = host.rendering().sceneOutputSize();
                    const luna::editor::Vec3 camera_position = host.viewport().editorCameraPosition();
                    const bool runtime_viewport = host.runtimeViewport().isRuntimeViewportEnabled();
                    const std::vector<luna::editor::RenderFeatureInfo> render_features =
                        host.rendering().defaultRenderFeatureInfos();
                    const std::optional<SceneVisibilityStats> visibility_stats =
                        findSceneVisibilityStats(render_features);

                    ui.heading("Rendering", std::string("Luna RHI / ") + host.rendering().backendName());
                    if (ui.beginTable(
                            "##SceneStatusMetrics",
                            2,
                            static_cast<luna::editor::TableFlags>(luna::editor::TableFlag::SizingStretchProp))) {
                        ui.tableNextRow();
                        ui.tableNextColumn();
                        ui.metric("Frame", formatFloat(host.rendering().frameTimeMilliseconds(), 2) + " ms");
                        ui.tableNextColumn();
                        ui.metric("Rate",
                                  formatFloat(host.rendering().framesPerSecond(), 1) + " FPS",
                                  {},
                                  luna::editor::StatusVariant::Success);
                        ui.tableNextRow();
                        ui.tableNextColumn();
                        ui.metric("Entities",
                                  std::to_string(host.scene().entityCount()),
                                  runtime_viewport ? "Editor scene" : "Active scene",
                                  luna::editor::StatusVariant::Info);
                        ui.tableNextColumn();
                        ui.metric("Viewport",
                                  std::to_string(viewport_size.x) + " x " + std::to_string(viewport_size.y),
                                  runtime_viewport ? "Runtime" : "Editor",
                                  runtime_viewport ? luna::editor::StatusVariant::Warning
                                                   : luna::editor::StatusVariant::Info);
                        ui.endTable();
                    }

                    ui.heading("Scene");
                    ui.beginPanel("##SceneStatusScenePanel");
                    ui.keyValue("Scene File", host.scene().sceneLabel());
                    ui.keyValue("Mode", runtime_viewport ? "Runtime" : "Editor");
                    if (runtime_viewport) {
                        ui.keyValue("Runtime Entities", std::to_string(host.runtimeViewport().runtimeEntityCount()));
                    }
                    ui.endPanel();

                    if (visibility_stats.has_value()) {
                        drawVisibilityStatsPanel(host, ui, *visibility_stats);
                    }

                    ui.heading("Viewport");
                    ui.beginPanel("##SceneStatusViewportPanel");
                    ui.keyValue("Editor Camera", formatVec3(camera_position, 2));
                    ui.keyValue("Gizmo",
                                host.viewport().gizmoOperationName() + " / " + host.viewport().gizmoModeName());

                    bool pick_debug = host.viewport().pickDebugVisualizationEnabled();
                    if (ui.checkbox("Show Picking Debug", pick_debug)) {
                        host.viewport().setPickDebugVisualizationEnabled(pick_debug);
                    }

                    bool editor_grid = host.viewport().editorGridEnabled();
                    if (ui.checkbox("Show Editor Grid", editor_grid)) {
                        host.viewport().setEditorGridEnabled(editor_grid);
                    }
                    ui.endPanel();
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createSceneStatusPlugin()
{
    return std::make_unique<SceneStatusPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kSceneStatusPluginRegistration{
    kPluginId,
    createSceneStatusPlugin,
};

} // namespace

} // namespace luna::editor
