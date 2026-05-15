#include "Luna/Editor/Native/NativePlugin.h"

#include <cstddef>
#include <cstdio>

namespace {

namespace native = luna::editor::native;

constexpr const char* kPluginId = "com.example.native-template";
constexpr const char* kDisplayName = "Native Template Tool";
constexpr const char* kVersion = "0.1.0";
constexpr const char* kWindowId = "com.example.native-template.window";
constexpr const char* kCommandId = "com.example.native-template.open";

struct NativeTemplateState {
    int enabled{1};
    int click_count{0};
    char label[96]{"Native template state"};
    char asset_note[256]{};
};

NativeTemplateState g_state{};

int canOpenWindow(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    return host.windows().canSetOpen() ? 1 : 0;
}

int isWindowOpen(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    return host.windows().isOpen(kWindowId) ? 1 : 0;
}

void executeOpenWindow(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    host.windows().setOpen(kWindowId, true);
}

void drawWindow(void* window_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeTemplateState*>(window_user_data);
    if (state == nullptr) {
        return;
    }

    const native::Host host(host_api);
    const native::Ui ui = host.ui();
    if (!ui.canDrawText()) {
        return;
    }

    ui.text("Native editor plugin template");
    ui.textDisabled("This window uses only the Luna editor native SDK wrapper.");
    ui.separator();

    ui.checkbox("Enabled", &state->enabled);
    ui.inputTextWithHint("Label", "Plugin-local text", state->label, sizeof(state->label));

    if (ui.button("Run Tool Action", native::fillWidth(), LunaEditorButtonVariant_Primary)) {
        ++state->click_count;
        host.log().info("Native template action executed.");
    }

    char counter_text[64]{};
    std::snprintf(counter_text, sizeof(counter_text), "Actions: %d", state->click_count);
    ui.text(counter_text);

    ui.separatorText("Plugin Asset");
    if (state->asset_note[0] == '\0') {
        size_t required_size = 0;
        if (host.pluginAssets().readText("welcome.txt", nullptr, 0, &required_size) && required_size > 0) {
            (void) host.pluginAssets().readText("welcome.txt",
                                                state->asset_note,
                                                sizeof(state->asset_note),
                                                &required_size);
        }
    }

    if (state->asset_note[0] != '\0') {
        ui.textWrapped(state->asset_note);
    } else {
        ui.textDisabled("Plugin asset assets/welcome.txt was not found.");
    }
}

int onLoad(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeTemplateState*>(plugin_user_data);
    const native::Host host(host_api);
    if (state == nullptr || !host.valid()) {
        return 0;
    }

    if (!host.commands().canRegister() || !host.windows().canRegister() || !host.menus().canAdd() ||
        host.pluginAssets().native() == nullptr) {
        host.log().error("Native template requires command, window, menu, and plugin asset APIs.");
        return 0;
    }

    native::CommandDescriptor command{};
    command.id = kCommandId;
    command.label = "Open Native Template Tool";
    command.description = "Opens the native editor plugin template window.";
    command.shortcut = "";
    command.user_data = state;
    command.can_execute = &canOpenWindow;
    command.is_checked = &isWindowOpen;
    command.execute = &executeOpenWindow;

    if (!host.commands().registerCommand(command)) {
        host.log().error("Failed to register native template command.");
        return 0;
    }

    native::WindowDescriptor window{};
    window.id = kWindowId;
    window.title = "Native Template";
    window.default_open = true;
    window.default_size = native::vec2(320.0f, 180.0f);
    window.flags = LunaEditorWindowFlag_None;
    window.user_data = state;
    window.draw = &drawWindow;

    if (!host.windows().registerWindow(window)) {
        host.commands().unregisterCommand(kCommandId);
        host.log().error("Failed to register native template window.");
        return 0;
    }

    native::MenuItemDescriptor menu_item{};
    menu_item.menu_path = "Tools/Native Template";
    menu_item.command_id = kCommandId;
    menu_item.label = "Open Native Template Tool";
    menu_item.shortcut = "";

    if (!host.menus().addItem(menu_item)) {
        host.windows().unregisterWindow(kWindowId);
        host.commands().unregisterCommand(kCommandId);
        host.log().error("Failed to register native template menu item.");
        return 0;
    }

    host.log().info("Native template loaded.");
    return 1;
}

void onUnload(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    host.menus().removeItemsForCommand(kCommandId);
    host.windows().unregisterWindow(kWindowId);
    host.commands().unregisterCommand(kCommandId);
    host.log().info("Native template unloaded.");
}

} // namespace

extern "C" LUNA_EDITOR_PLUGIN_EXPORT int LunaCreateEditorPlugin(uint32_t host_api_version,
                                                                const LunaEditorHostApi* host_api,
                                                                LunaEditorPluginApi* out_plugin_api)
{
    if (!native::isCompatibleHost(host_api_version, host_api) || out_plugin_api == nullptr) {
        return 0;
    }

    native::PluginDescriptor plugin{};
    plugin.plugin_id = kPluginId;
    plugin.display_name = kDisplayName;
    plugin.version = kVersion;
    plugin.user_data = &g_state;
    plugin.on_load = &onLoad;
    plugin.on_unload = &onUnload;

    return native::fillPluginApi(plugin, out_plugin_api) ? 1 : 0;
}
