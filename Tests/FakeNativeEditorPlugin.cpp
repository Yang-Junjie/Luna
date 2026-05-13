#include "EditorApi/EditorNativePluginApi.h"

#if defined(LUNA_TEST_EDITOR_PLUGIN_MISSING_SYMBOL)

extern "C" LUNA_EDITOR_PLUGIN_EXPORT int LunaCreateWrongEditorPlugin()
{
    return 1;
}

#else

namespace {

constexpr const char* kExpectedPluginId = "luna.test.native";

#if defined(LUNA_TEST_EDITOR_PLUGIN_ID_MISMATCH)
constexpr const char* kPluginId = "luna.test.native.other";
#else
constexpr const char* kPluginId = kExpectedPluginId;
#endif

constexpr const char* kWindowId = "luna.test.native.window";
constexpr const char* kCommandId = "luna.test.native.open";

struct FakeNativeEditorPluginState {
    int command_execute_count{0};
    int draw_count{0};
    int unload_count{0};
};

FakeNativeEditorPluginState g_state{};

int canExecuteOpenWindow(void*, const LunaEditorHostApi* host_api)
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

void executeOpenWindow(void* command_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<FakeNativeEditorPluginState*>(command_user_data);
    if (state != nullptr) {
        ++state->command_execute_count;
    }
    if (host_api != nullptr && host_api->windows.set_window_open != nullptr) {
        host_api->windows.set_window_open(host_api->windows.api_user_data, kWindowId, 1);
    }
}

void drawWindow(void* window_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<FakeNativeEditorPluginState*>(window_user_data);
    if (state != nullptr) {
        ++state->draw_count;
    }
    if (host_api == nullptr) {
        return;
    }

    const LunaEditorUiApi& ui = host_api->ui;
    if (ui.text != nullptr) {
        ui.text(ui.api_user_data, "Fake native editor plugin window");
    }
    if (ui.button != nullptr && host_api->commands.execute_command != nullptr) {
        const LunaEditorVec2 size{};
        if (ui.button(ui.api_user_data, "Execute", &size, LunaEditorButtonVariant_Default) != 0) {
            host_api->commands.execute_command(host_api->commands.api_user_data, kCommandId);
        }
    }
}

int registerContributions(FakeNativeEditorPluginState* state, const LunaEditorHostApi* host_api)
{
    if (state == nullptr || host_api == nullptr || host_api->commands.register_command == nullptr ||
        host_api->windows.register_window == nullptr) {
        return 0;
    }

    LunaEditorCommandDescriptor command{};
    command.struct_size = sizeof(LunaEditorCommandDescriptor);
    command.api_version = LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION;
    command.id = kCommandId;
    command.label = "Open Fake Native Window";
    command.description = "Opens the fake native editor plugin test window.";
    command.shortcut = "";
    command.command_user_data = state;
    command.can_execute = &canExecuteOpenWindow;
    command.is_checked = &isWindowOpen;
    command.execute = &executeOpenWindow;

    if (host_api->commands.register_command(host_api->commands.api_user_data, &command) == 0) {
        return 0;
    }

    LunaEditorWindowDescriptor window{};
    window.struct_size = sizeof(LunaEditorWindowDescriptor);
    window.api_version = LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION;
    window.id = kWindowId;
    window.title = "Fake Native";
    window.default_open = 0;
    window.default_size = LunaEditorVec2{.x = 240.0f, .y = 180.0f};
    window.flags = LunaEditorWindowFlag_None;
    window.window_user_data = state;
    window.draw = &drawWindow;

    return host_api->windows.register_window(host_api->windows.api_user_data, &window);
}

int onLoad(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<FakeNativeEditorPluginState*>(plugin_user_data);
    if (registerContributions(state, host_api) == 0) {
        return 0;
    }

#if defined(LUNA_TEST_EDITOR_PLUGIN_LOAD_FAILURE)
    return 0;
#else
    return 1;
#endif
}

void onUnload(void* plugin_user_data, const LunaEditorHostApi*)
{
    auto* state = static_cast<FakeNativeEditorPluginState*>(plugin_user_data);
    if (state != nullptr) {
        ++state->unload_count;
    }
}

} // namespace

extern "C" LUNA_EDITOR_PLUGIN_EXPORT int LunaCreateEditorPlugin(uint32_t host_api_version,
                                                                const LunaEditorHostApi* host_api,
                                                                LunaEditorPluginApi* out_plugin_api)
{
    if (host_api_version != LUNA_EDITOR_HOST_API_VERSION || host_api == nullptr || out_plugin_api == nullptr) {
        return 0;
    }

    out_plugin_api->struct_size = sizeof(LunaEditorPluginApi);
#if defined(LUNA_TEST_EDITOR_PLUGIN_API_MISMATCH)
    out_plugin_api->api_version = LUNA_EDITOR_PLUGIN_API_VERSION + 1u;
#else
    out_plugin_api->api_version = LUNA_EDITOR_PLUGIN_API_VERSION;
#endif
    out_plugin_api->plugin_id = kPluginId;
    out_plugin_api->display_name = "Fake Native Editor Plugin";
    out_plugin_api->version = "0.1.0";
    out_plugin_api->plugin_user_data = &g_state;
#if defined(LUNA_TEST_EDITOR_PLUGIN_MISSING_CALLBACKS)
    out_plugin_api->on_load = nullptr;
    out_plugin_api->on_unload = nullptr;
#else
    out_plugin_api->on_load = &onLoad;
    out_plugin_api->on_unload = &onUnload;
#endif
    return 1;
}

#endif
