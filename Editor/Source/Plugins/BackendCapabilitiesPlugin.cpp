#include "Plugins/BackendCapabilitiesPlugin.h"

#include "EditorApi/EditorApi.h"

#include <string>
#include <string_view>

namespace {

constexpr const char* kPluginId = "luna.editor.backend-capabilities";
constexpr const char* kWindowId = "luna.editor.backend-capabilities.window";

const char* boolText(bool value)
{
    return value ? "Yes" : "No";
}

void setupTwoColumnTable(luna::editor::Ui& ui, std::string_view first_column, std::string_view second_column)
{
    ui.tableSetupColumn(first_column,
                        static_cast<luna::editor::TableColumnFlags>(
                            luna::editor::TableColumnFlag::WidthStretch));
    ui.tableSetupColumn(second_column,
                        static_cast<luna::editor::TableColumnFlags>(
                            luna::editor::TableColumnFlag::WidthFixed),
                        170.0f);
    ui.tableHeadersRow();
}

void textRow(luna::editor::Ui& ui, std::string_view name, std::string_view value)
{
    ui.tableNextRow();
    ui.tableNextColumn();
    ui.text(name);
    ui.tableNextColumn();
    ui.text(value);
}

void capabilityRow(luna::editor::Ui& ui, std::string_view name, bool value)
{
    textRow(ui, name, boolText(value));
}

bool beginTwoColumnTable(luna::editor::Ui& ui,
                         std::string_view id,
                         std::string_view first_column,
                         std::string_view second_column)
{
    const luna::editor::TableFlags table_flags =
        luna::editor::TableFlag::BordersInnerV | luna::editor::TableFlag::RowBg;
    if (!ui.beginTable(id, 2, table_flags)) {
        return false;
    }

    setupTwoColumnTable(ui, first_column, second_column);
    return true;
}

class BackendCapabilitiesPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Backend Capabilities",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Backend Capabilities",
            .default_open = false,
            .default_size = luna::editor::Vec2{.x = 430.0f, .y = 520.0f},
            .draw =
                [](luna::editor::WindowDrawContext& context) {
                    luna::editor::Ui& ui = context.ui();
                    const luna::editor::RenderingBackendCapabilities capabilities =
                        context.host().rendering().backendCapabilities();

                    ui.text("Backend: " + capabilities.active_backend_name);
                    ui.separator();
                    ui.text("Compiled: " + capabilities.compiled_backend_names);

                    if (beginTwoColumnTable(ui, "CompiledRHIBackendsTable", "Backend", "Status")) {
                        for (const luna::editor::RenderingBackendEntry& backend : capabilities.compiled_backends) {
                            textRow(ui, backend.name, backend.status);
                        }
                        ui.endTable();
                    }

                    ui.spacing();
                    if (beginTwoColumnTable(ui, "BackendCapabilitiesTable", "Capability", "Value")) {
                        capabilityRow(ui, "Default Render Flow", capabilities.supports_default_render_flow);
                        capabilityRow(ui, "ImGui", capabilities.supports_imgui);
                        capabilityRow(ui, "Scene Pick Readback", capabilities.supports_scene_pick_readback);
                        capabilityRow(ui, "GPU Timestamp", capabilities.supports_gpu_timestamp);
                        textRow(ui,
                                "GPU Timestamp Mode",
                                capabilities.supports_gpu_timestamp
                                    ? (capabilities.gpu_timestamp_uses_disjoint_query ? "Disjoint query"
                                                                                      : "Fixed period")
                                    : "Unavailable");
                        ui.endTable();
                    }

                    ui.spacing();
                    if (beginTwoColumnTable(ui, "BackendResourceCapabilitiesTable", "Resource", "Value")) {
                        capabilityRow(ui, "Graphics Pipeline", capabilities.supports_graphics_pipeline);
                        capabilityRow(ui, "Compute Pipeline", capabilities.supports_compute_pipeline);
                        capabilityRow(ui, "Sampled Texture", capabilities.supports_sampled_texture);
                        capabilityRow(ui, "Storage Texture", capabilities.supports_storage_texture);
                        capabilityRow(ui, "Color Attachment", capabilities.supports_color_attachment);
                        capabilityRow(ui, "Depth Attachment", capabilities.supports_depth_attachment);
                        capabilityRow(ui, "Uniform Buffer", capabilities.supports_uniform_buffer);
                        capabilityRow(ui, "Storage Buffer", capabilities.supports_storage_buffer);
                        capabilityRow(ui, "Sampler", capabilities.supports_sampler);
                        ui.endTable();
                    }

                    ui.spacing();
                    if (beginTwoColumnTable(ui, "BackendConventionsTable", "Convention", "Value")) {
                        capabilityRow(ui,
                                      "Projection Y Flip",
                                      capabilities.conventions.requires_projection_y_flip);
                        textRow(ui,
                                "ImGui Clip Top Y",
                                capabilities.conventions.imgui_clip_top_y_is_negative_one ? "-1" : "+1");
                        textRow(ui,
                                "ImGui Render Target UV",
                                capabilities.conventions.imgui_render_target_requires_uv_y_flip ? "Flip Y"
                                                                                                 : "Direct");
                        textRow(ui,
                                "Scene Pick Y",
                                capabilities.conventions.scene_pick_y_matches_display_y ? "Displayed Y"
                                                                                        : "Invert displayed Y");
                        ui.endTable();
                    }
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

std::unique_ptr<Plugin> createBackendCapabilitiesPlugin()
{
    return std::make_unique<BackendCapabilitiesPlugin>();
}

} // namespace luna::editor
