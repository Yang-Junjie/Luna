#include "Plugins/SceneStatus/SceneStatusPlugin.h"

#include "EditorApi/EditorApi.h"

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

                    ui.text("Backend: Luna RHI / " + host.rendering().backendName());
                    ui.text("Frame: " + formatFloat(host.rendering().frameTimeMilliseconds(), 2) + " ms  |  " +
                            formatFloat(host.rendering().framesPerSecond(), 1) + " FPS");
                    ui.separator();
                    ui.text("Scene File: " + host.scene().sceneLabel());
                    ui.text("Entities: " + std::to_string(host.scene().entityCount()));
                    ui.separator();

                    ui.text("Viewport: " + std::to_string(viewport_size.x) + " x " +
                            std::to_string(viewport_size.y));
                    ui.text(std::string("Viewport Mode: ") +
                            (host.runtimeViewport().isRuntimeViewportEnabled() ? "Runtime" : "Editor"));
                    if (host.runtimeViewport().isRuntimeViewportEnabled()) {
                        ui.text("Runtime Entities: " + std::to_string(host.runtimeViewport().runtimeEntityCount()));
                    }

                    ui.text("Editor Camera: " + formatVec3(camera_position, 2));
                    ui.text("Gizmo: " + host.viewport().gizmoOperationName() + " / " +
                            host.viewport().gizmoModeName());
                    ui.textDisabled("Gizmo shortcuts: W Translate, E Rotate, R Scale, Q Local/World.");

                    bool pick_debug = host.viewport().pickDebugVisualizationEnabled();
                    if (ui.checkbox("Show Picking Debug", pick_debug)) {
                        host.viewport().setPickDebugVisualizationEnabled(pick_debug);
                    }

                    bool editor_grid = host.viewport().editorGridEnabled();
                    if (ui.checkbox("Show Editor Grid", editor_grid)) {
                        host.viewport().setEditorGridEnabled(editor_grid);
                    }

                    ui.textDisabled("Highlights pickable pixels and shows the requested pick marker.");
                    ui.textDisabled(
                        "Scene rendering targets a persistent offscreen texture and is presented in the Viewport panel.");
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

} // namespace luna::editor
