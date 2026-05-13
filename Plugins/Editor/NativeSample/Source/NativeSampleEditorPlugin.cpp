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

    if (host_api->commands.register_command == nullptr || host_api->windows.register_window == nullptr) {
        logMessage(host_api, LunaEditorLogLevel_Error, "Native sample requires command and window host APIs.");
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

    logMessage(host_api, LunaEditorLogLevel_Info, "Native sample loaded.");
    return 1;
}

void unloadNativeSample(void*, const LunaEditorHostApi* host_api)
{
    if (host_api != nullptr) {
        if (host_api->windows.unregister_window != nullptr) {
            host_api->windows.unregister_window(host_api->windows.api_user_data, kWindowId);
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
