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
    if (host_api != nullptr && host_api->scene.create_entity != nullptr && host_api->selection.select_entity != nullptr) {
        const uint64_t entity_id = host_api->scene.create_entity(host_api->scene.api_user_data, "Fake Native Entity");
        if (entity_id != 0u) {
            host_api->selection.select_entity(host_api->selection.api_user_data, entity_id);
        }
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
        host_api->windows.register_window == nullptr || host_api->menus.add_menu_item == nullptr ||
        host_api->plugin_assets.read_text == nullptr || host_api->assets.describe_asset == nullptr ||
        host_api->project.has_project_loaded == nullptr || host_api->project.project_info == nullptr ||
        host_api->scene.scene_label == nullptr || host_api->scene.entity_count == nullptr ||
        host_api->scene.entity_info == nullptr || host_api->selection.selected_entity_id == nullptr ||
        host_api->scene.get_camera_component == nullptr || host_api->scene.set_camera_component == nullptr ||
        host_api->scene.get_light_component == nullptr || host_api->scene.set_light_component == nullptr ||
        host_api->scene.get_mesh_component == nullptr || host_api->scene.set_mesh_component == nullptr ||
        host_api->viewport.scene_texture_view == nullptr ||
        host_api->viewport.editor_camera_position == nullptr || host_api->viewport.gizmo_operation_name == nullptr ||
        host_api->viewport.gizmo_mode_name == nullptr ||
        host_api->viewport.pick_debug_visualization_enabled == nullptr ||
        host_api->viewport.set_pick_debug_visualization_enabled == nullptr ||
        host_api->viewport.editor_grid_enabled == nullptr || host_api->viewport.set_editor_grid_enabled == nullptr ||
        host_api->runtime_viewport.is_runtime_viewport_enabled == nullptr ||
        host_api->runtime_viewport.is_runtime_viewport_requested == nullptr ||
        host_api->runtime_viewport.set_runtime_viewport_requested == nullptr ||
        host_api->runtime_viewport.runtime_entity_count == nullptr) {
        return 0;
    }

    if (host_api->project.has_project_loaded(host_api->project.api_user_data) == 0) {
        return 0;
    }

    char project_name[64]{};
    LunaEditorProjectInfo project_info{};
    project_info.struct_size = sizeof(LunaEditorProjectInfo);
    project_info.api_version = LUNA_EDITOR_PROJECT_INFO_API_VERSION;
    project_info.name = project_name;
    project_info.name_size = sizeof(project_name);
    if (host_api->project.project_info(host_api->project.api_user_data, &project_info) == 0 ||
        project_name[0] == '\0') {
        return 0;
    }

    char scene_label[64]{};
    if (host_api->scene.scene_label(host_api->scene.api_user_data, scene_label, sizeof(scene_label)) == 0 ||
        host_api->scene.entity_count(host_api->scene.api_user_data) == 0u) {
        return 0;
    }

    char entity_name[64]{};
    LunaEditorSceneEntityInfo entity_info{};
    entity_info.struct_size = sizeof(LunaEditorSceneEntityInfo);
    entity_info.api_version = LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION;
    entity_info.name = entity_name;
    entity_info.name_size = sizeof(entity_name);
    if (host_api->scene.entity_info(host_api->scene.api_user_data, 1u, &entity_info) == 0 ||
        entity_info.id != 1u) {
        return 0;
    }

    LunaEditorSceneCameraComponent camera{};
    camera.struct_size = sizeof(LunaEditorSceneCameraComponent);
    camera.api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
    if (host_api->scene.get_camera_component(host_api->scene.api_user_data, 1u, &camera) == 0) {
        camera.primary = 1;
        camera.fixed_aspect_ratio = 0;
        camera.projection = 0;
        camera.perspective_vertical_fov_degrees = 60.0f;
        camera.perspective_near = 0.1f;
        camera.perspective_far = 1000.0f;
        camera.orthographic_size = 12.0f;
        camera.orthographic_near = -50.0f;
        camera.orthographic_far = 50.0f;
    }
    if (host_api->scene.set_camera_component(host_api->scene.api_user_data, 1u, &camera) == 0) {
        return 0;
    }

    LunaEditorSceneLightComponent light{};
    light.struct_size = sizeof(LunaEditorSceneLightComponent);
    light.api_version = LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION;
    if (host_api->scene.get_light_component(host_api->scene.api_user_data, 1u, &light) == 0) {
        light.type = 0;
        light.enabled = 1;
        light.color = LunaEditorVec3{.x = 1.0f, .y = 0.9f, .z = 0.8f};
        light.intensity = 3.0f;
        light.range = 20.0f;
        light.inner_cone_angle_degrees = 10.0f;
        light.outer_cone_angle_degrees = 20.0f;
    }
    if (host_api->scene.set_light_component(host_api->scene.api_user_data, 1u, &light) == 0) {
        return 0;
    }

    uint64_t materials[] = {99u};
    LunaEditorSceneMeshComponent mesh{};
    mesh.struct_size = sizeof(LunaEditorSceneMeshComponent);
    mesh.api_version = LUNA_EDITOR_SCENE_MESH_COMPONENT_API_VERSION;
    mesh.mesh_handle = 777u;
    mesh.first_submesh = 0u;
    mesh.submesh_count = 1u;
    mesh.submesh_material_handles = materials;
    mesh.submesh_material_capacity = 1u;
    mesh.submesh_material_count = 1u;
    if (host_api->scene.set_mesh_component(host_api->scene.api_user_data, 1u, &mesh) == 0) {
        return 0;
    }

    char plugin_asset_text[64]{};
    size_t plugin_asset_text_size = 0;
    if (host_api->plugin_assets.read_text(
            host_api->plugin_assets.api_user_data, "fixture.txt", plugin_asset_text, sizeof(plugin_asset_text), &plugin_asset_text_size) == 0 ||
        plugin_asset_text_size == 0) {
        return 0;
    }

    char asset_label[64]{};
    LunaEditorAssetInfo asset_info{};
    asset_info.struct_size = sizeof(LunaEditorAssetInfo);
    asset_info.api_version = LUNA_EDITOR_ASSET_INFO_API_VERSION;
    asset_info.label = asset_label;
    asset_info.label_size = sizeof(asset_label);
    if (host_api->assets.describe_asset(host_api->assets.api_user_data, 42u, &asset_info) == 0 ||
        asset_info.handle != 42u) {
        return 0;
    }

    LunaEditorTextureView texture{};
    if (host_api->viewport.scene_texture_view(host_api->viewport.api_user_data, &texture) == 0 || texture.texture_id == 0u) {
        return 0;
    }

    LunaEditorVec3 camera_position{};
    host_api->viewport.editor_camera_position(host_api->viewport.api_user_data, &camera_position);
    if (camera_position.x == 0.0f && camera_position.y == 0.0f && camera_position.z == 0.0f) {
        return 0;
    }

    char gizmo_operation[32]{};
    char gizmo_mode[32]{};
    if (host_api->viewport.gizmo_operation_name(host_api->viewport.api_user_data,
                                                gizmo_operation,
                                                sizeof(gizmo_operation)) == 0 ||
        host_api->viewport.gizmo_mode_name(host_api->viewport.api_user_data, gizmo_mode, sizeof(gizmo_mode)) == 0 ||
        gizmo_operation[0] == '\0' || gizmo_mode[0] == '\0') {
        return 0;
    }

    host_api->viewport.set_pick_debug_visualization_enabled(host_api->viewport.api_user_data, 1);
    if (host_api->viewport.pick_debug_visualization_enabled(host_api->viewport.api_user_data) == 0) {
        return 0;
    }
    host_api->viewport.set_editor_grid_enabled(host_api->viewport.api_user_data, 0);
    if (host_api->viewport.editor_grid_enabled(host_api->viewport.api_user_data) != 0) {
        return 0;
    }

    const int runtime_viewport_enabled =
        host_api->runtime_viewport.is_runtime_viewport_enabled(host_api->runtime_viewport.api_user_data);
    (void) runtime_viewport_enabled;
    host_api->runtime_viewport.set_runtime_viewport_requested(host_api->runtime_viewport.api_user_data, 1);
    if (host_api->runtime_viewport.is_runtime_viewport_requested(host_api->runtime_viewport.api_user_data) == 0 ||
        host_api->runtime_viewport.runtime_entity_count(host_api->runtime_viewport.api_user_data) == 0u) {
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

    if (host_api->windows.register_window(host_api->windows.api_user_data, &window) == 0) {
        return 0;
    }

    LunaEditorMenuItemDescriptor menu_item{};
    menu_item.struct_size = sizeof(LunaEditorMenuItemDescriptor);
    menu_item.api_version = LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION;
    menu_item.menu_path = "Tools/Fake Native";
    menu_item.command_id = kCommandId;
    menu_item.label = "Open Fake Native Window";
    menu_item.shortcut = "";

    return host_api->menus.add_menu_item(host_api->menus.api_user_data, &menu_item);
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
