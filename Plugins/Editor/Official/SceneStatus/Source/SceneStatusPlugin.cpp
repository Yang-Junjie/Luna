#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"
#include "SceneStatusPlugin.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace {

constexpr const char* kPluginId = "luna.editor.scene-status";
constexpr const char* kWindowId = "luna.editor.scene-status.window";

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

                    ui.heading("Rendering",
                               std::string("Luna RHI / ") + host.rendering().backendName());
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
