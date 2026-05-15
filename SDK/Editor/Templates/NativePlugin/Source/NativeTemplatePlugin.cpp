#include "EditorApi/EditorNativePluginApi.h"

#include <cstdio>

namespace {

constexpr const char* kPluginId = "com.example.native-template";
constexpr const char* kDisplayName = "Native Template Tool";
constexpr const char* kVersion = "0.1.0";
constexpr const char* kWindowId = "com.example.native-template.window";
constexpr const char* kCommandId = "com.example.native-template.open";

struct NativeTemplateState {
    int click_count{0};
};

NativeTemplateState g_state{};

void logMessage(const LunaEditorHostApi* host_api, LunaEditorLogLevel level, const char* message)
{
    if (host_api != nullptr && host_api->log.log != nullptr) {
        host_api->log.log(host_api->log.api_user_data, level, message);
    }
}

int canOpenWindow(void*, const LunaEditorHostApi* host_api)
{
    return host_api != nullptr && host_api->windows.set_window_open != nullptr ? 1 : 0;
}

int isWindowOpen(void*, const LunaEditorHostApi* host_api)
{
    if (host_api == nullptr || host_api->windows.is_window_open == nullptr) {
        return 0;
    }
    return host_api->windows.is_window_open(host_api->windows.api_user_data, kWindowId);
}

void executeOpenWindow(void*, const LunaEditorHostApi* host_api)
{
    if (host_api != nullptr && host_api->windows.set_window_open != nullptr) {
        host_api->windows.set_window_open(host_api->windows.api_user_data, kWindowId, 1);
    }
}

void drawWindow(void* window_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeTemplateState*>(window_user_data);
    if (state == nullptr || host_api == nullptr) {
        return;
    }

    const LunaEditorUiApi& ui = host_api->ui;
    if (ui.text == nullptr) {
        return;
    }

    ui.text(ui.api_user_data, "Native editor plugin template");
    ui.text_disabled(ui.api_user_data, "This window uses only EditorNativePluginApi.h.");
    ui.separator(ui.api_user_data);

    if (ui.button != nullptr) {
        const LunaEditorVec2 button_size{.x = -1.0f, .y = 0.0f};
        if (ui.button(ui.api_user_data, "Run Tool Action", &button_size, LunaEditorButtonVariant_Primary) != 0) {
            ++state->click_count;
            logMessage(host_api, LunaEditorLogLevel_Info, "Native template action executed.");
        }
    }

    char counter_text[64]{};
    std::snprintf(counter_text, sizeof(counter_text), "Actions: %d", state->click_count);
    ui.text(ui.api_user_data, counter_text);
}

int onLoad(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeTemplateState*>(plugin_user_data);
    if (state == nullptr || host_api == nullptr) {
        return 0;
    }

    if (host_api->commands.register_command == nullptr || host_api->windows.register_window == nullptr ||
        host_api->menus.add_menu_item == nullptr) {
        logMessage(host_api, LunaEditorLogLevel_Error, "Native template requires command, window, and menu APIs.");
        return 0;
    }

    LunaEditorCommandDescriptor command{};
    command.struct_size = sizeof(LunaEditorCommandDescriptor);
    command.api_version = LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION;
    command.id = kCommandId;
    command.label = "Open Native Template Tool";
    command.description = "Opens the native editor plugin template window.";
    command.shortcut = "";
    command.command_user_data = state;
    command.can_execute = &canOpenWindow;
    command.is_checked = &isWindowOpen;
    command.execute = &executeOpenWindow;

    if (host_api->commands.register_command(host_api->commands.api_user_data, &command) == 0) {
        logMessage(host_api, LunaEditorLogLevel_Error, "Failed to register native template command.");
        return 0;
    }

    LunaEditorWindowDescriptor window{};
    window.struct_size = sizeof(LunaEditorWindowDescriptor);
    window.api_version = LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION;
    window.id = kWindowId;
    window.title = "Native Template";
    window.default_open = 1;
    window.default_size = LunaEditorVec2{.x = 320.0f, .y = 180.0f};
    window.flags = LunaEditorWindowFlag_None;
    window.window_user_data = state;
    window.draw = &drawWindow;

    if (host_api->windows.register_window(host_api->windows.api_user_data, &window) == 0) {
        host_api->commands.unregister_command(host_api->commands.api_user_data, kCommandId);
        logMessage(host_api, LunaEditorLogLevel_Error, "Failed to register native template window.");
        return 0;
    }

    LunaEditorMenuItemDescriptor menu_item{};
    menu_item.struct_size = sizeof(LunaEditorMenuItemDescriptor);
    menu_item.api_version = LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION;
    menu_item.menu_path = "Tools/Native Template";
    menu_item.command_id = kCommandId;
    menu_item.label = "Open Native Template Tool";
    menu_item.shortcut = "";

    if (host_api->menus.add_menu_item(host_api->menus.api_user_data, &menu_item) == 0) {
        host_api->windows.unregister_window(host_api->windows.api_user_data, kWindowId);
        host_api->commands.unregister_command(host_api->commands.api_user_data, kCommandId);
        logMessage(host_api, LunaEditorLogLevel_Error, "Failed to register native template menu item.");
        return 0;
    }

    logMessage(host_api, LunaEditorLogLevel_Info, "Native template loaded.");
    return 1;
}

void onUnload(void*, const LunaEditorHostApi* host_api)
{
    if (host_api != nullptr) {
        if (host_api->menus.remove_menu_items_for_command != nullptr) {
            host_api->menus.remove_menu_items_for_command(host_api->menus.api_user_data, kCommandId);
        }
        if (host_api->windows.unregister_window != nullptr) {
            host_api->windows.unregister_window(host_api->windows.api_user_data, kWindowId);
        }
        if (host_api->commands.unregister_command != nullptr) {
            host_api->commands.unregister_command(host_api->commands.api_user_data, kCommandId);
        }
    }
    logMessage(host_api, LunaEditorLogLevel_Info, "Native template unloaded.");
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
    out_plugin_api->on_load = &onLoad;
    out_plugin_api->on_unload = &onUnload;
    return 1;
}
