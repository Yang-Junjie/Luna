#include "Luna/Editor/Native/NativePlugin.h"

#include <cstdio>

namespace {

namespace native = luna::editor::native;

constexpr const char* kPluginId = "luna.source.native-sample";
constexpr const char* kDisplayName = "Native Sample Tool";
constexpr const char* kVersion = "0.1.0";
constexpr const char* kWindowId = "luna.source.native-sample.window";
constexpr const char* kCommandId = "luna.source.native-sample.open";

struct NativeSampleState {
    int enabled{1};
    int action_count{0};
    float intensity{0.5f};
    native::Vec3 accent{0.2f, 0.7f, 0.9f};
    char label[96]{"Native sample state"};
    char asset_note[256]{};
    uint64_t viewport_id{0};
};

NativeSampleState g_state{};

int canOpenSampleWindow(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    return host.windows().canSetOpen() ? 1 : 0;
}

int isSampleWindowOpen(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    return host.windows().isOpen(kWindowId) ? 1 : 0;
}

void executeOpenSampleWindow(void* command_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(command_user_data);
    if (state != nullptr) {
        ++state->action_count;
    }

    const native::Host host(host_api);
    host.windows().setOpen(kWindowId, true);
    host.log().info("Native sample command executed.");
}

void drawProjectInfo(const native::Host& host, const native::Ui& ui)
{
    char project_name[128]{};
    char assets_path[192]{};
    LunaEditorProjectInfo project_info{};
    project_info.struct_size = sizeof(LunaEditorProjectInfo);
    project_info.api_version = LUNA_EDITOR_PROJECT_INFO_API_VERSION;
    project_info.name = project_name;
    project_info.name_size = sizeof(project_name);
    project_info.assets_path = assets_path;
    project_info.assets_path_size = sizeof(assets_path);

    if (host.project().info(&project_info)) {
        char project_text[256]{};
        std::snprintf(project_text, sizeof(project_text), "Project: %s (%s)", project_name, assets_path);
        ui.text(project_text);
    } else if (!host.project().hasProjectLoaded()) {
        ui.textDisabled("No project loaded.");
    }
}

void drawSceneInfo(const native::Host& host, const native::Ui& ui)
{
    char scene_label[128]{};
    (void) host.scene().label(scene_label, sizeof(scene_label));

    char scene_text[256]{};
    std::snprintf(scene_text,
                  sizeof(scene_text),
                  "Scene: %s (%llu entities)",
                  scene_label[0] != '\0' ? scene_label : "Untitled",
                  static_cast<unsigned long long>(host.scene().entityCount()));
    ui.text(scene_text);

    const uint64_t selected_entity = host.selection().selectedEntityId();
    char selection_text[96]{};
    std::snprintf(selection_text,
                  sizeof(selection_text),
                  "Selected entity: %llu",
                  static_cast<unsigned long long>(selected_entity));
    ui.text(selection_text);
}

void drawViewportInfo(NativeSampleState& state, const native::Host& host, const native::Ui& ui)
{
    const native::Viewport viewport = host.viewport();
    const native::Vec3 camera_position = viewport.editorCameraPosition();
    char gizmo_operation[32]{};
    char gizmo_mode[32]{};
    (void) viewport.gizmoOperationName(gizmo_operation, sizeof(gizmo_operation));
    (void) viewport.gizmoModeName(gizmo_mode, sizeof(gizmo_mode));

    char viewport_text[256]{};
    std::snprintf(viewport_text,
                  sizeof(viewport_text),
                  "Editor Camera: %.2f, %.2f, %.2f | Gizmo: %s / %s",
                  camera_position.x,
                  camera_position.y,
                  camera_position.z,
                  gizmo_operation[0] != '\0' ? gizmo_operation : "Unknown",
                  gizmo_mode[0] != '\0' ? gizmo_mode : "Unknown");
    ui.text(viewport_text);

    native::TextureView texture{};
    if (viewport.sceneTextureView(&texture)) {
        char viewport_size_text[128]{};
        std::snprintf(viewport_size_text,
                      sizeof(viewport_size_text),
                      "Scene Texture: %ux%u (%s)",
                      texture.width,
                      texture.height,
                      texture.texture_id != 0u ? "available" : "missing");
        ui.text(viewport_size_text);
    }

    if (state.viewport_id == 0u || !viewport.isSceneViewportValid(state.viewport_id)) {
        state.viewport_id = viewport.createSceneViewport("NativeSampleViewport");
    }

    if (state.viewport_id != 0u) {
        LunaEditorViewportPresentation presentation = native::makeViewportPresentation();
        if (viewport.syncSceneViewport(state.viewport_id, 320u, 180u, &presentation)) {
            char plugin_viewport_text[128]{};
            std::snprintf(plugin_viewport_text,
                          sizeof(plugin_viewport_text),
                          "Plugin Viewport: %ux%u (%s)",
                          presentation.framebuffer_width,
                          presentation.framebuffer_height,
                          presentation.scene_texture.texture_id != 0u ? "available" : "missing");
            ui.text(plugin_viewport_text);
        }
    }

    int pick_debug = viewport.pickDebugVisualizationEnabled() ? 1 : 0;
    if (ui.checkbox("Viewport Pick Debug", &pick_debug)) {
        viewport.setPickDebugVisualizationEnabled(pick_debug != 0);
    }

    int editor_grid = viewport.editorGridEnabled() ? 1 : 0;
    if (ui.checkbox("Viewport Grid", &editor_grid)) {
        viewport.setEditorGridEnabled(editor_grid != 0);
    }
}

void drawRuntimeViewportInfo(const native::Host& host, const native::Ui& ui)
{
    const native::RuntimeViewport runtime_viewport = host.runtimeViewport();
    char runtime_text[160]{};
    std::snprintf(runtime_text,
                  sizeof(runtime_text),
                  "Runtime Viewport: %s / requested=%s / entities=%llu",
                  runtime_viewport.enabled() ? "enabled" : "editor",
                  runtime_viewport.requested() ? "true" : "false",
                  static_cast<unsigned long long>(runtime_viewport.entityCount()));
    ui.text(runtime_text);

    int runtime_requested = runtime_viewport.requested() ? 1 : 0;
    if (ui.checkbox("Request Runtime Viewport", &runtime_requested)) {
        runtime_viewport.setRequested(runtime_requested != 0);
    }
}

void drawSceneActions(const native::Host& host, const native::Ui& ui)
{
    if (ui.button("Create Native Entity", {}, LunaEditorButtonVariant_Subtle)) {
        const uint64_t entity_id = host.scene().createEntity("Native Sample Entity");
        if (entity_id != 0u) {
            host.selection().selectEntity(entity_id);
        }
    }

    const uint64_t selected_entity = host.selection().selectedEntityId();
    if (selected_entity == 0u) {
        return;
    }

    LunaEditorSceneCameraComponent camera{};
    camera.struct_size = sizeof(LunaEditorSceneCameraComponent);
    camera.api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
    const bool has_camera = host.scene().getCameraComponent(selected_entity, &camera);
    ui.text(has_camera ? "Selected entity has a camera component." : "Selected entity has no camera component.");

    if (ui.button("Ensure Camera Component")) {
        if (!has_camera) {
            camera = native::makeDefaultPerspectiveCamera();
        }
        (void) host.scene().setCameraComponent(selected_entity, camera);
    }
}

void drawPluginAssetInfo(NativeSampleState& state, const native::Host& host, const native::Ui& ui)
{
    if (state.asset_note[0] == '\0') {
        size_t required_size = 0;
        if (host.pluginAssets().readText("welcome.txt", nullptr, 0, &required_size) && required_size > 0) {
            (void) host.pluginAssets().readText("welcome.txt", state.asset_note, sizeof(state.asset_note), &required_size);
        }
    }

    if (state.asset_note[0] != '\0') {
        ui.textWrapped(state.asset_note);
    } else {
        ui.textDisabled("Plugin asset welcome.txt was not found.");
    }
}

void drawApiTable(const native::Host& host, const native::Ui& ui)
{
    if (!ui.canDrawTable()) {
        return;
    }

    const uint32_t table_flags = LunaEditorTableFlag_RowBg | LunaEditorTableFlag_BordersInnerH |
                                 LunaEditorTableFlag_SizingStretchProp;
    if (!ui.beginTable("native-sample-api-table", 2, table_flags)) {
        return;
    }

    ui.tableSetupColumn("Name", LunaEditorTableColumnFlag_WidthFixed, 140.0f);
    ui.tableSetupColumn("Value", LunaEditorTableColumnFlag_WidthStretch, 1.0f);
    ui.tableHeadersRow();

    ui.tableNextRow();
    ui.tableNextColumn();
    ui.text("Host API");
    ui.tableNextColumn();
    char host_version_text[32]{};
    std::snprintf(host_version_text, sizeof(host_version_text), "%u", host.native()->api_version);
    ui.text(host_version_text);

    ui.tableNextRow();
    ui.tableNextColumn();
    ui.text("Plugin API");
    ui.tableNextColumn();
    char plugin_version_text[32]{};
    std::snprintf(plugin_version_text, sizeof(plugin_version_text), "%u", LUNA_EDITOR_PLUGIN_API_VERSION);
    ui.text(plugin_version_text);

    ui.endTable();
}

void drawNativeSampleWindow(void* window_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(window_user_data);
    const native::Host host(host_api);
    if (state == nullptr || !host.valid()) {
        return;
    }

    const native::Ui ui = host.ui();
    if (!ui.canDrawText()) {
        return;
    }

    ui.text("This window is drawn by a dynamically loaded native editor plugin.");
    ui.textDisabled("It uses Luna/Editor/Native C++ wrappers over EditorNativePluginApi.h.");
    ui.separator();

    if (ui.button("Execute Registered Command", native::fillWidth(), LunaEditorButtonVariant_Primary)) {
        (void) host.commands().execute(kCommandId);
    }

    ui.separatorText("State");
    ui.checkbox("Enabled", &state->enabled);
    ui.sliderFloat("Intensity", &state->intensity, 0.0f, 1.0f, "%.2f");
    ui.colorEdit3("Accent", &state->accent);
    ui.inputTextWithHint("Label", "Plugin-local text", state->label, sizeof(state->label));

    char counter_text[64]{};
    std::snprintf(counter_text, sizeof(counter_text), "Command executions: %d", state->action_count);
    ui.text(counter_text);

    ui.separatorText("Host API");
    drawProjectInfo(host, ui);
    drawSceneInfo(host, ui);
    drawViewportInfo(*state, host, ui);
    drawRuntimeViewportInfo(host, ui);
    drawSceneActions(host, ui);
    drawPluginAssetInfo(*state, host, ui);

    char revision_text[64]{};
    std::snprintf(revision_text,
                  sizeof(revision_text),
                  "Asset revision: %llu",
                  static_cast<unsigned long long>(host.assets().revision()));
    ui.text(revision_text);

    drawApiTable(host, ui);
}

int loadNativeSample(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(plugin_user_data);
    const native::Host host(host_api);
    if (state == nullptr || !host.valid()) {
        return 0;
    }

    if (!host.commands().canRegister() || !host.windows().canRegister() || !host.menus().canAdd() ||
        host.pluginAssets().native() == nullptr) {
        host.log().error("Native sample requires command, window, menu, and plugin asset host APIs.");
        return 0;
    }

    native::CommandDescriptor command{};
    command.id = kCommandId;
    command.label = "Open Native Sample Tool";
    command.description = "Opens the dynamically loaded native sample editor window.";
    command.shortcut = "";
    command.user_data = state;
    command.can_execute = &canOpenSampleWindow;
    command.is_checked = &isSampleWindowOpen;
    command.execute = &executeOpenSampleWindow;

    if (!host.commands().registerCommand(command)) {
        host.log().error("Failed to register native sample command.");
        return 0;
    }

    native::WindowDescriptor window{};
    window.id = kWindowId;
    window.title = "Native Sample";
    window.default_open = false;
    window.default_size = native::vec2(360.0f, 320.0f);
    window.flags = LunaEditorWindowFlag_None;
    window.user_data = state;
    window.draw = &drawNativeSampleWindow;

    if (!host.windows().registerWindow(window)) {
        host.commands().unregisterCommand(kCommandId);
        host.log().error("Failed to register native sample window.");
        return 0;
    }

    native::MenuItemDescriptor menu_item{};
    menu_item.menu_path = "Tools/Native Sample";
    menu_item.command_id = kCommandId;
    menu_item.label = "Open Native Sample Tool";
    menu_item.shortcut = "";

    if (!host.menus().addItem(menu_item)) {
        host.windows().unregisterWindow(kWindowId);
        host.commands().unregisterCommand(kCommandId);
        host.log().error("Failed to register native sample menu item.");
        return 0;
    }

    host.log().info("Native sample loaded.");
    return 1;
}

void unloadNativeSample(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    if (g_state.viewport_id != 0u) {
        host.viewport().destroySceneViewport(g_state.viewport_id);
        g_state.viewport_id = 0u;
    }

    host.windows().unregisterWindow(kWindowId);
    host.menus().removeItemsForCommand(kCommandId);
    host.commands().unregisterCommand(kCommandId);
    host.log().info("Native sample unloaded.");
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
    plugin.on_load = &loadNativeSample;
    plugin.on_unload = &unloadNativeSample;

    return native::fillPluginApi(plugin, out_plugin_api) ? 1 : 0;
}
