#include "EditorApi/EditorNativePluginApi.h"

#include <cstdio>

namespace {

constexpr const char* kPluginId = "luna.source.native-sample";
constexpr const char* kDisplayName = "Native Sample Tool";
constexpr const char* kVersion = "0.1.0";
constexpr const char* kWindowId = "luna.source.native-sample.window";
constexpr const char* kCommandId = "luna.source.native-sample.open";

struct NativeSampleState {
    int enabled{1};
    int action_count{0};
    float intensity{0.5f};
    LunaEditorVec3 accent{0.2f, 0.7f, 0.9f};
    char label[96]{"Native sample state"};
    char asset_note[256]{};
};

NativeSampleState g_state{};

void logMessage(const LunaEditorHostApi* host_api, LunaEditorLogLevel level, const char* message)
{
    if (host_api == nullptr || host_api->log.log == nullptr) {
        return;
    }
    host_api->log.log(host_api->log.api_user_data, level, message);
}

int canOpenSampleWindow(void*, const LunaEditorHostApi* host_api)
{
    return host_api != nullptr && host_api->windows.set_window_open != nullptr ? 1 : 0;
}

int isSampleWindowOpen(void*, const LunaEditorHostApi* host_api)
{
    if (host_api == nullptr || host_api->windows.is_window_open == nullptr) {
        return 0;
    }
    return host_api->windows.is_window_open(host_api->windows.api_user_data, kWindowId);
}

void executeOpenSampleWindow(void* command_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(command_user_data);
    if (state != nullptr) {
        ++state->action_count;
    }

    if (host_api != nullptr && host_api->windows.set_window_open != nullptr) {
        host_api->windows.set_window_open(host_api->windows.api_user_data, kWindowId, 1);
    }
    logMessage(host_api, LunaEditorLogLevel_Info, "Native sample command executed.");
}

void drawNativeSampleWindow(void* window_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(window_user_data);
    if (state == nullptr || host_api == nullptr) {
        return;
    }

    const LunaEditorUiApi& ui = host_api->ui;
    if (ui.text == nullptr) {
        return;
    }

    ui.text(ui.api_user_data, "This window is drawn by a dynamically loaded native editor plugin.");
    ui.text_disabled(ui.api_user_data, "It uses only Luna/EditorApi/EditorNativePluginApi.h.");
    ui.separator(ui.api_user_data);

    if (ui.button != nullptr) {
        LunaEditorVec2 full_width{.x = -1.0f, .y = 0.0f};
        if (ui.button(ui.api_user_data, "Execute Registered Command", &full_width, LunaEditorButtonVariant_Primary) != 0 &&
            host_api->commands.execute_command != nullptr) {
            host_api->commands.execute_command(host_api->commands.api_user_data, kCommandId);
        }
    }

    if (ui.separator_text != nullptr) {
        ui.separator_text(ui.api_user_data, "State");
    }
    if (ui.checkbox != nullptr) {
        ui.checkbox(ui.api_user_data, "Enabled", &state->enabled);
    }
    if (ui.slider_float != nullptr) {
        ui.slider_float(ui.api_user_data, "Intensity", &state->intensity, 0.0f, 1.0f, "%.2f");
    }
    if (ui.color_edit3 != nullptr) {
        ui.color_edit3(ui.api_user_data, "Accent", &state->accent);
    }
    if (ui.input_text_with_hint != nullptr) {
        ui.input_text_with_hint(ui.api_user_data, "Label", "Plugin-local text", state->label, sizeof(state->label));
    }

    char counter_text[64]{};
    std::snprintf(counter_text, sizeof(counter_text), "Command executions: %d", state->action_count);
    ui.text(ui.api_user_data, counter_text);

    if (ui.separator_text != nullptr) {
        ui.separator_text(ui.api_user_data, "Host API");
    }

    if (host_api->project.project_info != nullptr) {
        char project_name[128]{};
        char assets_path[192]{};
        LunaEditorProjectInfo project_info{};
        project_info.struct_size = sizeof(LunaEditorProjectInfo);
        project_info.api_version = LUNA_EDITOR_PROJECT_INFO_API_VERSION;
        project_info.name = project_name;
        project_info.name_size = sizeof(project_name);
        project_info.assets_path = assets_path;
        project_info.assets_path_size = sizeof(assets_path);
        if (host_api->project.project_info(host_api->project.api_user_data, &project_info) != 0) {
            char project_text[256]{};
            std::snprintf(project_text, sizeof(project_text), "Project: %s (%s)", project_name, assets_path);
            ui.text(ui.api_user_data, project_text);
        } else if (host_api->project.has_project_loaded != nullptr &&
                   host_api->project.has_project_loaded(host_api->project.api_user_data) == 0) {
            ui.text_disabled(ui.api_user_data, "No project loaded.");
        }
    }

    if (host_api->scene.scene_label != nullptr && host_api->scene.entity_count != nullptr) {
        char scene_label[128]{};
        host_api->scene.scene_label(host_api->scene.api_user_data, scene_label, sizeof(scene_label));
        char scene_text[256]{};
        std::snprintf(scene_text,
                      sizeof(scene_text),
                      "Scene: %s (%llu entities)",
                      scene_label[0] != '\0' ? scene_label : "Untitled",
                      static_cast<unsigned long long>(host_api->scene.entity_count(host_api->scene.api_user_data)));
        ui.text(ui.api_user_data, scene_text);
    }

    if (host_api->selection.selected_entity_id != nullptr) {
        const uint64_t selected_entity = host_api->selection.selected_entity_id(host_api->selection.api_user_data);
        char selection_text[96]{};
        std::snprintf(selection_text,
                      sizeof(selection_text),
                      "Selected entity: %llu",
                      static_cast<unsigned long long>(selected_entity));
        ui.text(ui.api_user_data, selection_text);
    }

    if (host_api->viewport.editor_camera_position != nullptr && host_api->viewport.gizmo_operation_name != nullptr &&
        host_api->viewport.gizmo_mode_name != nullptr) {
        LunaEditorVec3 camera_position{};
        host_api->viewport.editor_camera_position(host_api->viewport.api_user_data, &camera_position);
        char gizmo_operation[32]{};
        char gizmo_mode[32]{};
        (void) host_api->viewport.gizmo_operation_name(
            host_api->viewport.api_user_data, gizmo_operation, sizeof(gizmo_operation));
        (void) host_api->viewport.gizmo_mode_name(host_api->viewport.api_user_data, gizmo_mode, sizeof(gizmo_mode));

        char viewport_text[256]{};
        std::snprintf(viewport_text,
                      sizeof(viewport_text),
                      "Editor Camera: %.2f, %.2f, %.2f | Gizmo: %s / %s",
                      camera_position.x,
                      camera_position.y,
                      camera_position.z,
                      gizmo_operation[0] != '\0' ? gizmo_operation : "Unknown",
                      gizmo_mode[0] != '\0' ? gizmo_mode : "Unknown");
        ui.text(ui.api_user_data, viewport_text);
    }

    if (host_api->viewport.scene_texture_view != nullptr) {
        LunaEditorTextureView texture{};
        if (host_api->viewport.scene_texture_view(host_api->viewport.api_user_data, &texture) != 0) {
            char viewport_size_text[128]{};
            std::snprintf(viewport_size_text,
                          sizeof(viewport_size_text),
                          "Scene Texture: %ux%u (%s)",
                          texture.width,
                          texture.height,
                          texture.texture_id != 0u ? "available" : "missing");
            ui.text(ui.api_user_data, viewport_size_text);
        }
    }

    if (host_api->runtime_viewport.is_runtime_viewport_enabled != nullptr &&
        host_api->runtime_viewport.is_runtime_viewport_requested != nullptr &&
        host_api->runtime_viewport.runtime_entity_count != nullptr) {
        const int runtime_enabled =
            host_api->runtime_viewport.is_runtime_viewport_enabled(host_api->runtime_viewport.api_user_data);
        const int runtime_requested =
            host_api->runtime_viewport.is_runtime_viewport_requested(host_api->runtime_viewport.api_user_data);
        char runtime_text[160]{};
        std::snprintf(runtime_text,
                      sizeof(runtime_text),
                      "Runtime Viewport: %s / requested=%s / entities=%llu",
                      runtime_enabled != 0 ? "enabled" : "editor",
                      runtime_requested != 0 ? "true" : "false",
                      static_cast<unsigned long long>(
                          host_api->runtime_viewport.runtime_entity_count(host_api->runtime_viewport.api_user_data)));
        ui.text(ui.api_user_data, runtime_text);
    }

    if (ui.checkbox != nullptr && host_api->viewport.pick_debug_visualization_enabled != nullptr &&
        host_api->viewport.set_pick_debug_visualization_enabled != nullptr) {
        int pick_debug = host_api->viewport.pick_debug_visualization_enabled(host_api->viewport.api_user_data);
        if (ui.checkbox(ui.api_user_data, "Viewport Pick Debug", &pick_debug) != 0) {
            host_api->viewport.set_pick_debug_visualization_enabled(
                host_api->viewport.api_user_data, pick_debug);
        }
    }

    if (ui.checkbox != nullptr && host_api->viewport.editor_grid_enabled != nullptr &&
        host_api->viewport.set_editor_grid_enabled != nullptr) {
        int editor_grid = host_api->viewport.editor_grid_enabled(host_api->viewport.api_user_data);
        if (ui.checkbox(ui.api_user_data, "Viewport Grid", &editor_grid) != 0) {
            host_api->viewport.set_editor_grid_enabled(host_api->viewport.api_user_data, editor_grid);
        }
    }

    if (ui.checkbox != nullptr && host_api->runtime_viewport.is_runtime_viewport_requested != nullptr &&
        host_api->runtime_viewport.set_runtime_viewport_requested != nullptr) {
        int runtime_requested =
            host_api->runtime_viewport.is_runtime_viewport_requested(host_api->runtime_viewport.api_user_data);
        if (ui.checkbox(ui.api_user_data, "Request Runtime Viewport", &runtime_requested) != 0) {
            host_api->runtime_viewport.set_runtime_viewport_requested(
                host_api->runtime_viewport.api_user_data, runtime_requested);
        }
    }

    if (ui.button != nullptr && host_api->scene.create_entity != nullptr && host_api->selection.select_entity != nullptr) {
        if (ui.button(ui.api_user_data, "Create Native Entity", nullptr, LunaEditorButtonVariant_Subtle) != 0) {
            const uint64_t entity_id =
                host_api->scene.create_entity(host_api->scene.api_user_data, "Native Sample Entity");
            if (entity_id != 0u) {
                host_api->selection.select_entity(host_api->selection.api_user_data, entity_id);
            }
        }
    }

    if (host_api->scene.get_camera_component != nullptr && host_api->scene.set_camera_component != nullptr &&
        host_api->selection.selected_entity_id != nullptr) {
        const uint64_t selected_entity = host_api->selection.selected_entity_id(host_api->selection.api_user_data);
        if (selected_entity != 0u) {
            LunaEditorSceneCameraComponent camera{};
            camera.struct_size = sizeof(LunaEditorSceneCameraComponent);
            camera.api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
            const bool has_camera =
                host_api->scene.get_camera_component(host_api->scene.api_user_data, selected_entity, &camera) != 0;
            const char* camera_status_text =
                has_camera ? "Selected entity has a camera component." : "Selected entity has no camera component.";
            ui.text(ui.api_user_data, camera_status_text);
            if (ui.button != nullptr &&
                ui.button(ui.api_user_data, "Ensure Camera Component", nullptr, LunaEditorButtonVariant_Default) != 0) {
                if (!has_camera) {
                    camera.primary = 1;
                    camera.fixed_aspect_ratio = 0;
                    camera.projection = 0;
                    camera.perspective_vertical_fov_degrees = 50.0f;
                    camera.perspective_near = 0.05f;
                    camera.perspective_far = 500.0f;
                    camera.orthographic_size = 10.0f;
                    camera.orthographic_near = -100.0f;
                    camera.orthographic_far = 100.0f;
                }
                (void) host_api->scene.set_camera_component(host_api->scene.api_user_data, selected_entity, &camera);
            }
        }
    }

    if (host_api->plugin_assets.read_text != nullptr && state->asset_note[0] == '\0') {
        size_t required_size = 0;
        if (host_api->plugin_assets.read_text(
                host_api->plugin_assets.api_user_data, "welcome.txt", nullptr, 0, &required_size) != 0 &&
            required_size > 0) {
            host_api->plugin_assets.read_text(host_api->plugin_assets.api_user_data,
                                              "welcome.txt",
                                              state->asset_note,
                                              sizeof(state->asset_note),
                                              &required_size);
        }
    }
    if (state->asset_note[0] != '\0') {
        ui.text_wrapped(ui.api_user_data, state->asset_note);
    } else {
        ui.text_disabled(ui.api_user_data, "Plugin asset welcome.txt was not found.");
    }

    if (host_api->assets.asset_revision != nullptr) {
        char revision_text[64]{};
        std::snprintf(revision_text,
                      sizeof(revision_text),
                      "Asset revision: %llu",
                      static_cast<unsigned long long>(
                          host_api->assets.asset_revision(host_api->assets.api_user_data)));
        ui.text(ui.api_user_data, revision_text);
    }

    if (ui.begin_table != nullptr && ui.end_table != nullptr && ui.table_next_row != nullptr &&
        ui.table_next_column != nullptr) {
        const LunaEditorVec2 outer_size{};
        const uint32_t table_flags = LunaEditorTableFlag_RowBg | LunaEditorTableFlag_BordersInnerH |
                                     LunaEditorTableFlag_SizingStretchProp;
        if (ui.begin_table(ui.api_user_data, "native-sample-api-table", 2, table_flags, &outer_size) != 0) {
            if (ui.table_setup_column != nullptr) {
                ui.table_setup_column(ui.api_user_data, "Name", LunaEditorTableColumnFlag_WidthFixed, 140.0f);
                ui.table_setup_column(ui.api_user_data, "Value", LunaEditorTableColumnFlag_WidthStretch, 1.0f);
            }
            if (ui.table_headers_row != nullptr) {
                ui.table_headers_row(ui.api_user_data);
            }

            ui.table_next_row(ui.api_user_data);
            ui.table_next_column(ui.api_user_data);
            ui.text(ui.api_user_data, "Host API");
            ui.table_next_column(ui.api_user_data);
            char host_version_text[32]{};
            std::snprintf(host_version_text, sizeof(host_version_text), "%u", host_api->api_version);
            ui.text(ui.api_user_data, host_version_text);

            ui.table_next_row(ui.api_user_data);
            ui.table_next_column(ui.api_user_data);
            ui.text(ui.api_user_data, "Plugin API");
            ui.table_next_column(ui.api_user_data);
            char plugin_version_text[32]{};
            std::snprintf(plugin_version_text, sizeof(plugin_version_text), "%u", LUNA_EDITOR_PLUGIN_API_VERSION);
            ui.text(ui.api_user_data, plugin_version_text);

            ui.end_table(ui.api_user_data);
        }
    }
}

int loadNativeSample(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(plugin_user_data);
    if (state == nullptr || host_api == nullptr) {
        return 0;
    }

    if (host_api->commands.register_command == nullptr || host_api->windows.register_window == nullptr ||
        host_api->menus.add_menu_item == nullptr || host_api->plugin_assets.read_text == nullptr) {
        logMessage(host_api, LunaEditorLogLevel_Error, "Native sample requires command, window, menu, and plugin asset host APIs.");
        return 0;
    }

    LunaEditorCommandDescriptor command{};
    command.struct_size = sizeof(LunaEditorCommandDescriptor);
    command.api_version = LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION;
    command.id = kCommandId;
    command.label = "Open Native Sample Tool";
    command.description = "Opens the dynamically loaded native sample editor window.";
    command.shortcut = "";
    command.command_user_data = state;
    command.can_execute = &canOpenSampleWindow;
    command.is_checked = &isSampleWindowOpen;
    command.execute = &executeOpenSampleWindow;

    if (host_api->commands.register_command(host_api->commands.api_user_data, &command) == 0) {
        logMessage(host_api, LunaEditorLogLevel_Error, "Failed to register native sample command.");
        return 0;
    }

    LunaEditorWindowDescriptor window{};
    window.struct_size = sizeof(LunaEditorWindowDescriptor);
    window.api_version = LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION;
    window.id = kWindowId;
    window.title = "Native Sample";
    window.default_open = 0;
    window.default_size = LunaEditorVec2{.x = 360.0f, .y = 320.0f};
    window.flags = LunaEditorWindowFlag_None;
    window.window_user_data = state;
    window.draw = &drawNativeSampleWindow;

    if (host_api->windows.register_window(host_api->windows.api_user_data, &window) == 0) {
        if (host_api->commands.unregister_command != nullptr) {
            host_api->commands.unregister_command(host_api->commands.api_user_data, kCommandId);
        }
        logMessage(host_api, LunaEditorLogLevel_Error, "Failed to register native sample window.");
        return 0;
    }

    LunaEditorMenuItemDescriptor menu_item{};
    menu_item.struct_size = sizeof(LunaEditorMenuItemDescriptor);
    menu_item.api_version = LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION;
    menu_item.menu_path = "Tools/Native Sample";
    menu_item.command_id = kCommandId;
    menu_item.label = "Open Native Sample Tool";
    menu_item.shortcut = "";

    if (host_api->menus.add_menu_item(host_api->menus.api_user_data, &menu_item) == 0) {
        host_api->windows.unregister_window(host_api->windows.api_user_data, kWindowId);
        host_api->commands.unregister_command(host_api->commands.api_user_data, kCommandId);
        logMessage(host_api, LunaEditorLogLevel_Error, "Failed to register native sample menu item.");
        return 0;
    }

    logMessage(host_api, LunaEditorLogLevel_Info, "Native sample loaded.");
    return 1;
}

void unloadNativeSample(void*, const LunaEditorHostApi* host_api)
{
    if (host_api != nullptr) {
        if (host_api->windows.unregister_window != nullptr) {
            host_api->windows.unregister_window(host_api->windows.api_user_data, kWindowId);
        }
        if (host_api->menus.remove_menu_items_for_command != nullptr) {
            host_api->menus.remove_menu_items_for_command(host_api->menus.api_user_data, kCommandId);
        }
        if (host_api->commands.unregister_command != nullptr) {
            host_api->commands.unregister_command(host_api->commands.api_user_data, kCommandId);
        }
    }
    logMessage(host_api, LunaEditorLogLevel_Info, "Native sample unloaded.");
}

} // namespace

extern "C" LUNA_EDITOR_PLUGIN_EXPORT int LunaCreateEditorPlugin(uint32_t host_api_version,
                                                                const LunaEditorHostApi* host_api,
                                                                LunaEditorPluginApi* out_plugin_api)
{
    if (host_api_version != LUNA_EDITOR_HOST_API_VERSION || host_api == nullptr ||
        host_api->struct_size < sizeof(LunaEditorHostApi) || host_api->api_version != LUNA_EDITOR_HOST_API_VERSION ||
        out_plugin_api == nullptr) {
        return 0;
    }

    out_plugin_api->struct_size = sizeof(LunaEditorPluginApi);
    out_plugin_api->api_version = LUNA_EDITOR_PLUGIN_API_VERSION;
    out_plugin_api->plugin_id = kPluginId;
    out_plugin_api->display_name = kDisplayName;
    out_plugin_api->version = kVersion;
    out_plugin_api->plugin_user_data = &g_state;
    out_plugin_api->on_load = &loadNativeSample;
    out_plugin_api->on_unload = &unloadNativeSample;
    return 1;
}
