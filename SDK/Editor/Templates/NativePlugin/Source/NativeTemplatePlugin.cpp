#include "Luna/Editor/Native/NativePlugin.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

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
    uint64_t created_entity_id{0};
    char label[96]{"Native template state"};
    std::string asset_note;
    native::RegisteredCommand command;
    native::RegisteredWindow window;
    native::RegisteredMenuItemsForCommand menu_items;
    native::SceneViewportHandle preview_viewport;

    void cleanup() noexcept
    {
        preview_viewport.reset();
        menu_items.reset();
        window.reset();
        command.reset();
    }
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

    ui.heading("Native Template", "Native editor plugin template");
    ui.beginPanel("NativeTemplateSummary");
    ui.keyValue("Boundary", "Luna editor native SDK wrapper only");
    ui.badge("Styled UI ABI", LunaEditorStatusVariant_Success);
    ui.endPanel();
    ui.separator();

    ui.checkbox("Enabled", &state->enabled);
    ui.inputTextWithHint("Label", "Plugin-local text", state->label, sizeof(state->label));
    ui.sameLine();
    ui.textDisabled("SDK wrapper parity");

    if (ui.beginSection("NativeTemplateControls", "Controls", true)) {
        int slider_value = state->click_count;
        ui.setNextItemWidth(ui.scale(180.0f));
        ui.sliderInt("Action Preview", &slider_value, 0, 32);

        native::Vec3 offset{.x = 0.0f, .y = 1.0f, .z = 2.0f};
        ui.dragFloat3("Offset", &offset, 0.05f, -10.0f, 10.0f);

        if (ui.beginCombo("Mode", state->enabled != 0 ? "Enabled" : "Disabled")) {
            if (ui.selectable("Enabled", state->enabled != 0)) {
                state->enabled = 1;
            }
            if (ui.selectable("Disabled", state->enabled == 0)) {
                state->enabled = 0;
            }
            ui.setItemDefaultFocus();
            ui.endCombo();
        }

        if (ui.treeNodeEx("NativeTemplateTree", "Scene Tool", LunaEditorTreeNodeFlag_DefaultOpen)) {
            ui.bulletText("Create entities through host.scene().");
            ui.treePop();
        }

        if (ui.beginDragDropSource()) {
            const uint64_t payload = state->created_entity_id;
            ui.setDragDropPayload("LUNA_TEMPLATE_ENTITY", &payload, sizeof(payload));
            ui.text("Entity payload");
            ui.endDragDropSource();
        }
        if (ui.beginDragDropTarget()) {
            uint64_t payload = 0u;
            (void) ui.acceptDragDropPayload("LUNA_TEMPLATE_ENTITY", &payload, sizeof(payload));
            ui.endDragDropTarget();
        }

        ui.endSection();
    }

    if (ui.button("Create Entity", native::fillWidth(), LunaEditorButtonVariant_Primary)) {
        ++state->click_count;
        state->created_entity_id = host.scene().createEntity("Native Template Entity");
        if (state->created_entity_id != 0u) {
            host.selection().selectEntity(state->created_entity_id);
            host.log().info("Native template created and selected an entity.");
        } else {
            host.log().warn("Native template could not create an entity.");
        }
    }

    char counter_text[64]{};
    std::snprintf(counter_text, sizeof(counter_text), "Actions: %d", state->click_count);
    ui.text(counter_text);
    if (ui.isItemHovered()) {
        ui.setTooltip("Native SDK item query wrapper");
    }

    if (state->created_entity_id != 0u && host.scene().entityExists(state->created_entity_id)) {
        const native::SceneEntityInfo entity_info = host.scene().entityInfo(state->created_entity_id);
        ui.textWrapped(("Created entity: " + entity_info.name).c_str());
    }

    ui.separatorText("Plugin Asset");
    if (state->asset_note.empty()) {
        state->asset_note = host.pluginAssets().readText("welcome.txt");
    }

    if (!state->asset_note.empty()) {
        ui.textWrapped(state->asset_note.c_str());
    } else {
        ui.textDisabled("Plugin asset assets/welcome.txt was not found.");
    }

    ui.separatorText("Project");
    if (host.project().hasProjectLoaded()) {
        const native::ProjectInfo project_info = host.project().info();
        ui.text(("Project: " + project_info.name).c_str());
        ui.textWrapped(("Root: " + host.project().rootPath()).c_str());
        ui.textWrapped(("Assets: " + host.assets().rootPath()).c_str());
    } else {
        ui.textDisabled("No project is loaded.");
    }

    const std::vector<native::AssetInfo> assets = host.assets().list(LunaEditorAssetType_None, false);
    char asset_count_text[96]{};
    std::snprintf(asset_count_text, sizeof(asset_count_text), "Project assets visible to SDK: %zu", assets.size());
    ui.metric("Project Assets",
              asset_count_text,
              "Enumerated through host.assets()",
              assets.empty() ? LunaEditorStatusVariant_Warning : LunaEditorStatusVariant_Info,
              native::vec2(-1.0f, 0.0f));
    if (!assets.empty()) {
        const native::AssetInfo& first_asset = assets.front();
        const std::string asset_detail =
            first_asset.detail.empty() ? first_asset.project_path : first_asset.detail + " / " + first_asset.project_path;
        (void) ui.assetField("NativeTemplateFirstAsset",
                             first_asset.label.empty() ? first_asset.project_path.c_str() : first_asset.label.c_str(),
                             asset_detail.c_str(),
                             first_asset.loading ? LunaEditorStatusVariant_Warning : LunaEditorStatusVariant_Info,
                             native::vec2(-1.0f, 0.0f));
        if (ui.isItemHovered()) {
            ui.setTooltip("Styled asset field from the native UI ABI.");
        }
        if (ui.beginDragDropTarget()) {
            native::AssetDropPayload dropped_asset{};
            const uint32_t accepted_types[] = {static_cast<uint32_t>(first_asset.type)};
            (void) ui.acceptAssetDragDropPayload(&dropped_asset, accepted_types);
            ui.endDragDropTarget();
        }
    } else {
        ui.emptyState("No project assets", "No project assets are visible to this SDK template.");
    }

    ui.separatorText("Viewport");
    if (!state->preview_viewport) {
        state->preview_viewport = host.viewport().createScopedSceneViewport("NativeTemplatePreview");
    }
    const native::Vec2 available = ui.contentRegionAvail();
    const float viewport_width = available.x > 64.0f ? available.x : 320.0f;
    const float viewport_height = viewport_width * 0.5625f;
    LunaEditorViewportPresentation presentation = native::makeViewportPresentation();
    if (state->preview_viewport &&
        host.viewport().syncSceneViewport(state->preview_viewport.id(),
                                          static_cast<uint32_t>(viewport_width),
                                          static_cast<uint32_t>(viewport_height),
                                          &presentation) &&
        presentation.presentable != 0) {
        ui.image(presentation.scene_texture, native::vec2(viewport_width, viewport_height));
    } else {
        ui.textDisabled("Scene viewport preview is not available.");
    }

    char runtime_text[96]{};
    std::snprintf(runtime_text,
                  sizeof(runtime_text),
                  "Runtime requested: %s, entities: %zu",
                  host.runtimeViewport().requested() ? "yes" : "no",
                  host.runtimeViewport().entityCount());
    ui.text(runtime_text);
}

int onLoad(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeTemplateState*>(plugin_user_data);
    const native::Host host(host_api);
    if (state == nullptr || !host.valid()) {
        return 0;
    }
    state->cleanup();
    state->click_count = 0;
    state->created_entity_id = 0u;
    state->asset_note.clear();

    if (!host.commands().canRegister() || !host.windows().canRegister() || !host.menus().canAdd() ||
        !host.pluginAssets().available() || !host.assets().available() || !host.scene().available() ||
        !host.selection().available() || !host.viewport().available()) {
        host.log().error("Native template requires command, window, menu, asset, scene, selection, and viewport APIs.");
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

    native::RegisteredCommand command_registration = host.commands().registerScoped(command);
    if (!command_registration) {
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

    native::RegisteredWindow window_registration = host.windows().registerScoped(window);
    if (!window_registration) {
        host.log().error("Failed to register native template window.");
        return 0;
    }

    native::MenuItemDescriptor menu_item{};
    menu_item.menu_path = "Tools/Native Template";
    menu_item.command_id = kCommandId;
    menu_item.label = "Open Native Template Tool";
    menu_item.shortcut = "";

    native::RegisteredMenuItemsForCommand menu_registration = host.menus().addScopedItemsForCommand(menu_item);
    if (!menu_registration) {
        host.log().error("Failed to register native template menu item.");
        return 0;
    }

    state->command = std::move(command_registration);
    state->window = std::move(window_registration);
    state->menu_items = std::move(menu_registration);

    host.log().info("Native template loaded.");
    return 1;
}

void onUnload(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeTemplateState*>(plugin_user_data);
    const native::Host host(host_api);
    if (state != nullptr) {
        state->cleanup();
    }
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
