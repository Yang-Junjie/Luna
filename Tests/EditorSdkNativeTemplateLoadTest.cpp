#include "Core/Log.h"
#include "EditorApi/EditorNativePluginApi.h"
#include "Platform/Common/DynamicLibrary.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char* kExpectedPluginId = "com.example.native-template";
constexpr const char* kExpectedWindowId = "com.example.native-template.window";
constexpr const char* kExpectedCommandId = "com.example.native-template.open";

struct CommandRecord {
    std::string id;
    void* user_data{};
    int (*can_execute)(void*, const LunaEditorHostApi*){};
    int (*is_checked)(void*, const LunaEditorHostApi*){};
    void (*execute)(void*, const LunaEditorHostApi*){};
};

struct WindowRecord {
    std::string id;
    bool open{};
    void* user_data{};
    void (*draw)(void*, const LunaEditorHostApi*){};
};

struct MenuRecord {
    std::string menu_path;
    std::string command_id;
};

struct EntityRecord {
    uint64_t id{};
    std::string name;
};

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (condition) {
            return true;
        }

        ++m_failures;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    [[nodiscard]] int result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures{};
};

struct TemplateHost {
    LunaEditorHostApi api{};
    std::unordered_map<std::string, CommandRecord> commands;
    std::unordered_map<std::string, WindowRecord> windows;
    std::vector<MenuRecord> menus;
    std::unordered_map<uint64_t, EntityRecord> entities;
    std::vector<std::string> logs;
    std::string plugin_root_path{"F:/SdkTemplatePlugin"};
    std::string plugin_asset_root_path{"F:/SdkTemplatePlugin/assets"};
    std::string project_root_path{"F:/SdkTemplateProject"};
    std::string assets_root_path{"F:/SdkTemplateProject/Assets"};
    uint64_t selected_entity_id{};
    uint64_t next_entity_id{100u};
    uint64_t next_viewport_id{2u};
    uint64_t created_viewport_id{};
    int text_count{};
    int image_count{};
    int plugin_asset_read_text_count{};
    int project_info_count{};
    int list_assets_count{};
    int created_entity_count{};
    bool next_button_pressed{};

    TemplateHost()
    {
        entities.emplace(1u, EntityRecord{.id = 1u, .name = "Root"});

        api.struct_size = sizeof(LunaEditorHostApi);
        api.api_version = LUNA_EDITOR_HOST_API_VERSION;
        api.host_user_data = this;
        api.log = LunaEditorLogApi{
            .struct_size = sizeof(LunaEditorLogApi),
            .api_version = LUNA_EDITOR_LOG_API_VERSION,
            .api_user_data = this,
            .log = &log,
        };
        api.ui = LunaEditorUiApi{
            .struct_size = sizeof(LunaEditorUiApi),
            .api_version = LUNA_EDITOR_UI_API_VERSION,
            .api_user_data = this,
            .text = &text,
            .text_disabled = &text,
            .text_wrapped = &text,
            .separator = &separator,
            .separator_text = &separatorText,
            .content_region_avail = &contentRegionAvail,
            .button = &button,
            .checkbox = &checkbox,
            .input_text_with_hint = &inputTextWithHint,
            .image = &image,
        };
        api.commands = LunaEditorCommandApi{
            .struct_size = sizeof(LunaEditorCommandApi),
            .api_version = LUNA_EDITOR_COMMAND_API_VERSION,
            .api_user_data = this,
            .register_command = &registerCommand,
            .unregister_command = &unregisterCommand,
            .execute_command = &executeCommand,
            .can_execute_command = &canExecuteCommand,
            .is_command_checked = &isCommandChecked,
        };
        api.windows = LunaEditorWindowApi{
            .struct_size = sizeof(LunaEditorWindowApi),
            .api_version = LUNA_EDITOR_WINDOW_API_VERSION,
            .api_user_data = this,
            .register_window = &registerWindow,
            .unregister_window = &unregisterWindow,
            .is_window_open = &isWindowOpen,
            .set_window_open = &setWindowOpen,
        };
        api.assets = LunaEditorAssetApi{
            .struct_size = sizeof(LunaEditorAssetApi),
            .api_version = LUNA_EDITOR_ASSET_API_VERSION,
            .api_user_data = this,
            .describe_asset = &describeAsset,
            .asset_info = &describeAsset,
            .asset_info_by_path = &assetInfoByPath,
            .list_assets = &listAssets,
            .asset_exists = &assetExists,
            .asset_path_exists = &assetPathExists,
            .find_asset_handle_by_path = &findAssetHandleByPath,
            .assets_root_path = &assetsRootPath,
            .resolve_project_asset_path = &resolveProjectAssetPath,
            .make_project_relative_asset_path = &makeProjectRelativeAssetPath,
            .refresh_assets = &refreshAssets,
            .asset_revision = &assetRevision,
            .is_asset_loading = &isAssetLoading,
            .accepts_asset_type = &acceptsAssetType,
            .mesh_submesh_count = &meshSubmeshCount,
            .begin_asset_drag_drop_source = &beginAssetDragDropSource,
        };
        api.plugin_assets = LunaEditorPluginAssetApi{
            .struct_size = sizeof(LunaEditorPluginAssetApi),
            .api_version = LUNA_EDITOR_PLUGIN_ASSET_API_VERSION,
            .api_user_data = this,
            .plugin_root_path = &pluginRootPath,
            .asset_root_path = &pluginAssetRootPath,
            .resolve_path = &pluginAssetResolvePath,
            .exists = &pluginAssetExists,
            .read_text = &pluginAssetReadText,
            .read_bytes = &pluginAssetReadBytes,
            .texture = &pluginAssetTexture,
        };
        api.menus = LunaEditorMenuApi{
            .struct_size = sizeof(LunaEditorMenuApi),
            .api_version = LUNA_EDITOR_MENU_API_VERSION,
            .api_user_data = this,
            .add_menu_item = &addMenuItem,
            .remove_menu_item = &removeMenuItem,
            .remove_menu_items_for_command = &removeMenuItemsForCommand,
        };
        api.project = LunaEditorProjectApi{
            .struct_size = sizeof(LunaEditorProjectApi),
            .api_version = LUNA_EDITOR_PROJECT_API_VERSION,
            .api_user_data = this,
            .has_project_loaded = &hasProjectLoaded,
            .project_root_path = &projectRootPath,
            .project_info = &projectInfo,
            .save_project = &saveProject,
        };
        api.scene = LunaEditorSceneApi{
            .struct_size = sizeof(LunaEditorSceneApi),
            .api_version = LUNA_EDITOR_SCENE_API_VERSION,
            .api_user_data = this,
            .scene_label = &sceneLabel,
            .entity_count = &entityCount,
            .can_edit_scene = &canEditScene,
            .open_scene_file = &openSceneFile,
            .enumerate_entities = &enumerateEntities,
            .entity_exists = &entityExists,
            .entity_info = &entityInfo,
            .is_entity_descendant_of = &isEntityDescendantOf,
            .create_entity = &createEntity,
            .create_entity_ex = &createEntityEx,
            .destroy_entity = &destroyEntity,
            .reparent_entity = &reparentEntity,
            .set_entity_name = &setEntityName,
            .get_entity_transform = &getEntityTransform,
            .set_entity_transform = &setEntityTransform,
            .get_camera_component = &getCameraComponent,
            .set_camera_component = &setCameraComponent,
            .get_light_component = &getLightComponent,
            .set_light_component = &setLightComponent,
            .get_mesh_component = &getMeshComponent,
            .set_mesh_component = &setMeshComponent,
            .add_component = &addComponent,
            .remove_component = &removeComponent,
        };
        api.selection = LunaEditorSelectionApi{
            .struct_size = sizeof(LunaEditorSelectionApi),
            .api_version = LUNA_EDITOR_SELECTION_API_VERSION,
            .api_user_data = this,
            .selected_entity_id = &selectedEntityId,
            .select_entity = &selectEntity,
            .clear_selection = &clearSelection,
        };
        api.viewport = LunaEditorViewportApi{
            .struct_size = sizeof(LunaEditorViewportApi),
            .api_version = LUNA_EDITOR_VIEWPORT_API_VERSION,
            .api_user_data = this,
            .sync_scene_viewport = &syncSceneViewport,
            .scene_texture_view = &sceneTextureView,
            .editor_camera_position = &editorCameraPosition,
            .gizmo_operation_name = &gizmoOperationName,
            .gizmo_mode_name = &gizmoModeName,
            .pick_debug_visualization_enabled = &pickDebugVisualizationEnabled,
            .set_pick_debug_visualization_enabled = &setPickDebugVisualizationEnabled,
            .editor_grid_enabled = &editorGridEnabled,
            .set_editor_grid_enabled = &setEditorGridEnabled,
            .default_scene_viewport = &defaultSceneViewport,
            .create_scene_viewport = &createSceneViewport,
            .destroy_scene_viewport = &destroySceneViewport,
            .is_scene_viewport_valid = &isSceneViewportValid,
            .sync_scene_viewport_ex = &syncSceneViewportEx,
            .scene_texture_view_ex = &sceneTextureViewEx,
        };
        api.runtime_viewport = LunaEditorRuntimeViewportApi{
            .struct_size = sizeof(LunaEditorRuntimeViewportApi),
            .api_version = LUNA_EDITOR_RUNTIME_VIEWPORT_API_VERSION,
            .api_user_data = this,
            .is_runtime_viewport_enabled = &isRuntimeViewportEnabled,
            .is_runtime_viewport_requested = &isRuntimeViewportRequested,
            .set_runtime_viewport_requested = &setRuntimeViewportRequested,
            .runtime_entity_count = &runtimeEntityCount,
        };
    }

    bool drawWindow(std::string_view id)
    {
        const auto it = windows.find(std::string(id));
        if (it == windows.end() || it->second.draw == nullptr) {
            return false;
        }
        it->second.draw(it->second.user_data, &api);
        return true;
    }

    static TemplateHost* self(void* api_user_data)
    {
        return static_cast<TemplateHost*>(api_user_data);
    }

    static void copyToBuffer(char* out_value, size_t out_value_size, std::string_view value)
    {
        if (out_value == nullptr || out_value_size == 0u) {
            return;
        }

        const size_t copy_size = (std::min)(out_value_size - 1u, value.size());
        if (copy_size > 0u) {
            std::memcpy(out_value, value.data(), copy_size);
        }
        out_value[copy_size] = '\0';
    }

    static std::string copyString(const char* value)
    {
        return value != nullptr ? std::string(value) : std::string{};
    }

    static void log(void* api_user_data, LunaEditorLogLevel, const char* message)
    {
        if (TemplateHost* host = self(api_user_data)) {
            host->logs.push_back(copyString(message));
        }
    }

    static void text(void* api_user_data, const char*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->text_count;
        }
    }

    static void separator(void*) {}
    static void separatorText(void*, const char*) {}

    static void contentRegionAvail(void*, LunaEditorVec2* out_value)
    {
        if (out_value != nullptr) {
            *out_value = LunaEditorVec2{.x = 320.0f, .y = 180.0f};
        }
    }

    static int button(void* api_user_data, const char*, const LunaEditorVec2*, uint32_t)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0;
        }
        const bool pressed = host->next_button_pressed;
        host->next_button_pressed = false;
        return pressed ? 1 : 0;
    }

    static int checkbox(void*, const char*, int*)
    {
        return 0;
    }

    static int inputTextWithHint(void*, const char*, const char*, char*, size_t)
    {
        return 0;
    }

    static int image(void* api_user_data, const LunaEditorTextureView* texture, const LunaEditorVec2*)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || texture == nullptr || texture->texture_id == 0u) {
            return 0;
        }
        ++host->image_count;
        return 1;
    }

    static int registerCommand(void* api_user_data, const LunaEditorCommandDescriptor* descriptor)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr ||
            descriptor->struct_size < sizeof(LunaEditorCommandDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION || descriptor->id == nullptr ||
            descriptor->execute == nullptr) {
            return 0;
        }

        host->commands[descriptor->id] = CommandRecord{
            .id = descriptor->id,
            .user_data = descriptor->command_user_data,
            .can_execute = descriptor->can_execute,
            .is_checked = descriptor->is_checked,
            .execute = descriptor->execute,
        };
        return 1;
    }

    static void unregisterCommand(void* api_user_data, const char* id)
    {
        if (TemplateHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->commands.erase(id);
        }
    }

    static int executeCommand(void* api_user_data, const char* id)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        if (it == host->commands.end() || it->second.execute == nullptr) {
            return 0;
        }
        if (it->second.can_execute != nullptr && it->second.can_execute(it->second.user_data, &host->api) == 0) {
            return 0;
        }

        it->second.execute(it->second.user_data, &host->api);
        return 1;
    }

    static int canExecuteCommand(void* api_user_data, const char* id)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        return it != host->commands.end() &&
                       (it->second.can_execute == nullptr || it->second.can_execute(it->second.user_data, &host->api) != 0)
                   ? 1
                   : 0;
    }

    static int isCommandChecked(void* api_user_data, const char* id)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        return it != host->commands.end() && it->second.is_checked != nullptr &&
                       it->second.is_checked(it->second.user_data, &host->api) != 0
                   ? 1
                   : 0;
    }

    static int registerWindow(void* api_user_data, const LunaEditorWindowDescriptor* descriptor)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr ||
            descriptor->struct_size < sizeof(LunaEditorWindowDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION || descriptor->id == nullptr ||
            descriptor->draw == nullptr) {
            return 0;
        }

        host->windows[descriptor->id] = WindowRecord{
            .id = descriptor->id,
            .open = descriptor->default_open != 0,
            .user_data = descriptor->window_user_data,
            .draw = descriptor->draw,
        };
        return 1;
    }

    static void unregisterWindow(void* api_user_data, const char* id)
    {
        if (TemplateHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->windows.erase(id);
        }
    }

    static int isWindowOpen(void* api_user_data, const char* id)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->windows.find(id);
        return it != host->windows.end() && it->second.open ? 1 : 0;
    }

    static void setWindowOpen(void* api_user_data, const char* id, int open)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return;
        }

        const auto it = host->windows.find(id);
        if (it != host->windows.end()) {
            it->second.open = open != 0;
        }
    }

    static bool fillAssetInfo(TemplateHost& host, uint64_t handle, LunaEditorAssetInfo* out_info)
    {
        if (out_info == nullptr || out_info->struct_size < sizeof(LunaEditorAssetInfo) ||
            out_info->api_version != LUNA_EDITOR_ASSET_INFO_API_VERSION) {
            return false;
        }

        out_info->handle = handle;
        out_info->type = LunaEditorAssetType_Texture;
        out_info->exists = 1;
        copyToBuffer(out_info->label, out_info->label_size, "Template Asset");
        copyToBuffer(out_info->detail, out_info->detail_size, "Texture");
        copyToBuffer(out_info->project_path, out_info->project_path_size, "Assets/Template.png");
        copyToBuffer(out_info->absolute_path, out_info->absolute_path_size, host.assets_root_path + "/Template.png");
        return true;
    }

    static int describeAsset(void* api_user_data, uint64_t handle, LunaEditorAssetInfo* out_info)
    {
        TemplateHost* host = self(api_user_data);
        return host != nullptr && fillAssetInfo(*host, handle, out_info) ? 1 : 0;
    }

    static int assetInfoByPath(void* api_user_data, const char*, LunaEditorAssetInfo* out_info)
    {
        return describeAsset(api_user_data, 42u, out_info);
    }

    static size_t listAssets(void* api_user_data,
                             uint32_t,
                             int,
                             void* user_data,
                             LunaEditorEnumerateAssetFn enumerate_fn)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        ++host->list_assets_count;
        if (enumerate_fn == nullptr) {
            return 1u;
        }

        char label[64]{};
        char detail[64]{};
        char project_path[128]{};
        char absolute_path[256]{};
        LunaEditorAssetInfo info{};
        info.struct_size = sizeof(LunaEditorAssetInfo);
        info.api_version = LUNA_EDITOR_ASSET_INFO_API_VERSION;
        info.label = label;
        info.label_size = sizeof(label);
        info.detail = detail;
        info.detail_size = sizeof(detail);
        info.project_path = project_path;
        info.project_path_size = sizeof(project_path);
        info.absolute_path = absolute_path;
        info.absolute_path_size = sizeof(absolute_path);
        return fillAssetInfo(*host, 42u, &info) && enumerate_fn(user_data, &info) != 0 ? 1u : 0u;
    }

    static int assetExists(void*, uint64_t handle) { return handle != 0u ? 1 : 0; }
    static int assetPathExists(void*, const char* path) { return path != nullptr ? 1 : 0; }
    static uint64_t findAssetHandleByPath(void*, const char* path) { return path != nullptr ? 42u : 0u; }

    static int assetsRootPath(void* api_user_data, char* out_path, size_t out_path_size)
    {
        if (TemplateHost* host = self(api_user_data)) {
            copyToBuffer(out_path, out_path_size, host->assets_root_path);
            return 1;
        }
        return 0;
    }

    static int resolveProjectAssetPath(void* api_user_data, const char* path, char* out_path, size_t out_path_size)
    {
        if (TemplateHost* host = self(api_user_data); host != nullptr && path != nullptr) {
            copyToBuffer(out_path, out_path_size, host->assets_root_path + "/" + path);
            return 1;
        }
        return 0;
    }

    static int makeProjectRelativeAssetPath(void*, const char* path, char* out_path, size_t out_path_size)
    {
        if (path == nullptr) {
            return 0;
        }
        copyToBuffer(out_path, out_path_size, path);
        return 1;
    }

    static int refreshAssets(void*, LunaEditorAssetRefreshResult* out_result)
    {
        if (out_result != nullptr && out_result->struct_size >= sizeof(LunaEditorAssetRefreshResult) &&
            out_result->api_version == LUNA_EDITOR_ASSET_REFRESH_RESULT_API_VERSION) {
            out_result->success = 1;
            out_result->project_loaded = 1;
            out_result->revision = 7u;
            copyToBuffer(out_result->message, out_result->message_size, "refreshed");
        }
        return 1;
    }

    static uint64_t assetRevision(void*) { return 7u; }
    static int isAssetLoading(void*, uint64_t) { return 0; }
    static int acceptsAssetType(void*, uint32_t, const uint32_t*, size_t) { return 1; }

    static int meshSubmeshCount(void*, uint64_t, size_t* out_count)
    {
        if (out_count != nullptr) {
            *out_count = 1u;
        }
        return 1;
    }

    static int beginAssetDragDropSource(void*, uint64_t, const char*) { return 1; }

    static int pluginRootPath(void* api_user_data, char* out_path, size_t out_path_size)
    {
        if (TemplateHost* host = self(api_user_data)) {
            copyToBuffer(out_path, out_path_size, host->plugin_root_path);
            return 1;
        }
        return 0;
    }

    static int pluginAssetRootPath(void* api_user_data, char* out_path, size_t out_path_size)
    {
        if (TemplateHost* host = self(api_user_data)) {
            copyToBuffer(out_path, out_path_size, host->plugin_asset_root_path);
            return 1;
        }
        return 0;
    }

    static int pluginAssetResolvePath(void* api_user_data,
                                      const char* relative_asset_path,
                                      char* out_path,
                                      size_t out_path_size)
    {
        if (TemplateHost* host = self(api_user_data); host != nullptr && relative_asset_path != nullptr) {
            copyToBuffer(out_path, out_path_size, host->plugin_asset_root_path + "/" + relative_asset_path);
            return 1;
        }
        return 0;
    }

    static int pluginAssetExists(void*, const char* relative_asset_path)
    {
        return relative_asset_path != nullptr && std::string_view(relative_asset_path) == "welcome.txt" ? 1 : 0;
    }

    static int pluginAssetReadText(void* api_user_data,
                                   const char* relative_asset_path,
                                   char* out_text,
                                   size_t out_text_size,
                                   size_t* out_required_size)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || !pluginAssetExists(api_user_data, relative_asset_path)) {
            return 0;
        }

        ++host->plugin_asset_read_text_count;
        constexpr std::string_view kText = "SDK template welcome text";
        if (out_required_size != nullptr) {
            *out_required_size = kText.size() + 1u;
        }
        copyToBuffer(out_text, out_text_size, kText);
        return out_text == nullptr || out_text_size >= kText.size() + 1u ? 1 : 0;
    }

    static int pluginAssetReadBytes(void*, const char* relative_asset_path, void* out_data, size_t out_data_size, size_t* out_required_size)
    {
        if (!pluginAssetExists(nullptr, relative_asset_path)) {
            return 0;
        }
        constexpr uint8_t kBytes[] = {1u, 2u, 3u};
        if (out_required_size != nullptr) {
            *out_required_size = sizeof(kBytes);
        }
        if (out_data != nullptr && out_data_size >= sizeof(kBytes)) {
            std::memcpy(out_data, kBytes, sizeof(kBytes));
        }
        return 1;
    }

    static int pluginAssetTexture(void*, const char* relative_asset_path, LunaEditorTextureView* out_texture)
    {
        if (relative_asset_path == nullptr || out_texture == nullptr) {
            return 0;
        }
        *out_texture = LunaEditorTextureView{.texture_id = 0x99u, .width = 16u, .height = 16u, .y_flip = 0};
        return 1;
    }

    static int addMenuItem(void* api_user_data, const LunaEditorMenuItemDescriptor* descriptor)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr ||
            descriptor->struct_size < sizeof(LunaEditorMenuItemDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION ||
            descriptor->menu_path == nullptr || descriptor->command_id == nullptr) {
            return 0;
        }
        host->menus.push_back(MenuRecord{.menu_path = descriptor->menu_path, .command_id = descriptor->command_id});
        return 1;
    }

    static void removeMenuItem(void* api_user_data, const char* menu_path, const char* command_id)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || menu_path == nullptr || command_id == nullptr) {
            return;
        }
        host->menus.erase(std::remove_if(host->menus.begin(),
                                         host->menus.end(),
                                         [&](const MenuRecord& item) {
                                             return item.menu_path == menu_path && item.command_id == command_id;
                                         }),
                          host->menus.end());
    }

    static void removeMenuItemsForCommand(void* api_user_data, const char* command_id)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || command_id == nullptr) {
            return;
        }
        host->menus.erase(std::remove_if(host->menus.begin(),
                                         host->menus.end(),
                                         [&](const MenuRecord& item) {
                                             return item.command_id == command_id;
                                         }),
                          host->menus.end());
    }

    static int hasProjectLoaded(void*) { return 1; }

    static int projectRootPath(void* api_user_data, char* out_path, size_t out_path_size)
    {
        if (TemplateHost* host = self(api_user_data)) {
            copyToBuffer(out_path, out_path_size, host->project_root_path);
            return 1;
        }
        return 0;
    }

    static int projectInfo(void* api_user_data, LunaEditorProjectInfo* out_info)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || out_info == nullptr || out_info->struct_size < sizeof(LunaEditorProjectInfo) ||
            out_info->api_version != LUNA_EDITOR_PROJECT_INFO_API_VERSION) {
            return 0;
        }
        ++host->project_info_count;
        copyToBuffer(out_info->name, out_info->name_size, "SDK Template Project");
        copyToBuffer(out_info->version, out_info->version_size, "0.1.0");
        copyToBuffer(out_info->assets_path, out_info->assets_path_size, "Assets");
        return 1;
    }

    static int saveProject(void*) { return 1; }

    static int sceneLabel(void*, char* out_label, size_t out_label_size)
    {
        copyToBuffer(out_label, out_label_size, "SDK Template Scene");
        return 1;
    }

    static size_t entityCount(void* api_user_data)
    {
        if (TemplateHost* host = self(api_user_data)) {
            return host->entities.size();
        }
        return 0u;
    }

    static int canEditScene(void*) { return 1; }
    static int openSceneFile(void*, const char*) { return 1; }

    static size_t enumerateEntities(void* api_user_data, void* user_data, LunaEditorEnumerateSceneEntityFn enumerate_fn)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        if (enumerate_fn == nullptr) {
            return host->entities.size();
        }
        size_t count = 0u;
        for (const auto& [_, entity] : host->entities) {
            char name[64]{};
            LunaEditorSceneEntityInfo info{};
            info.struct_size = sizeof(LunaEditorSceneEntityInfo);
            info.api_version = LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION;
            info.id = entity.id;
            info.name = name;
            info.name_size = sizeof(name);
            copyToBuffer(name, sizeof(name), entity.name);
            ++count;
            if (enumerate_fn(user_data, &info) == 0) {
                break;
            }
        }
        return count;
    }

    static int entityExists(void* api_user_data, uint64_t entity_id)
    {
        TemplateHost* host = self(api_user_data);
        return host != nullptr && host->entities.contains(entity_id) ? 1 : 0;
    }

    static int entityInfo(void* api_user_data, uint64_t entity_id, LunaEditorSceneEntityInfo* out_info)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || out_info == nullptr || out_info->struct_size < sizeof(LunaEditorSceneEntityInfo) ||
            out_info->api_version != LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        out_info->id = it->second.id;
        copyToBuffer(out_info->name, out_info->name_size, it->second.name);
        return 1;
    }

    static int isEntityDescendantOf(void*, uint64_t, uint64_t) { return 0; }

    static uint64_t createEntity(void* api_user_data, const char* name)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        const uint64_t id = host->next_entity_id++;
        host->entities.emplace(id, EntityRecord{.id = id, .name = name != nullptr ? name : "Entity"});
        ++host->created_entity_count;
        return id;
    }

    static uint64_t createEntityEx(void* api_user_data, const LunaEditorSceneEntityCreateRequest* request)
    {
        return createEntity(api_user_data, request != nullptr ? request->name : nullptr);
    }

    static int destroyEntity(void* api_user_data, uint64_t entity_id)
    {
        TemplateHost* host = self(api_user_data);
        return host != nullptr && host->entities.erase(entity_id) > 0u ? 1 : 0;
    }

    static int reparentEntity(void*, uint64_t, uint64_t, int) { return 1; }
    static int setEntityName(void*, uint64_t, const char*) { return 1; }
    static int getEntityTransform(void*, uint64_t, LunaEditorSceneTransform* out_transform)
    {
        if (out_transform != nullptr) {
            *out_transform = LunaEditorSceneTransform{};
        }
        return 1;
    }
    static int setEntityTransform(void*, uint64_t, const LunaEditorSceneTransform*) { return 1; }
    static int getCameraComponent(void*, uint64_t, LunaEditorSceneCameraComponent*) { return 0; }
    static int setCameraComponent(void*, uint64_t, const LunaEditorSceneCameraComponent*) { return 1; }
    static int getLightComponent(void*, uint64_t, LunaEditorSceneLightComponent*) { return 0; }
    static int setLightComponent(void*, uint64_t, const LunaEditorSceneLightComponent*) { return 1; }
    static int getMeshComponent(void*, uint64_t, LunaEditorSceneMeshComponent*) { return 0; }
    static int setMeshComponent(void*, uint64_t, const LunaEditorSceneMeshComponent*) { return 1; }
    static int addComponent(void*, uint64_t, uint32_t) { return 1; }
    static int removeComponent(void*, uint64_t, uint32_t) { return 1; }

    static uint64_t selectedEntityId(void* api_user_data)
    {
        if (TemplateHost* host = self(api_user_data)) {
            return host->selected_entity_id;
        }
        return 0u;
    }

    static void selectEntity(void* api_user_data, uint64_t entity_id)
    {
        if (TemplateHost* host = self(api_user_data)) {
            host->selected_entity_id = entity_id;
        }
    }

    static void clearSelection(void* api_user_data)
    {
        if (TemplateHost* host = self(api_user_data)) {
            host->selected_entity_id = 0u;
        }
    }

    static int syncSceneViewport(void*, uint32_t width, uint32_t height, LunaEditorViewportPresentation* out_presentation)
    {
        if (out_presentation == nullptr || out_presentation->struct_size < sizeof(LunaEditorViewportPresentation) ||
            out_presentation->api_version != LUNA_EDITOR_VIEWPORT_API_VERSION) {
            return 0;
        }
        out_presentation->scene_texture = LunaEditorTextureView{.texture_id = 0x1234u, .width = width, .height = height};
        out_presentation->framebuffer_width = width;
        out_presentation->framebuffer_height = height;
        out_presentation->presentable = 1;
        return 1;
    }

    static int sceneTextureView(void*, LunaEditorTextureView* out_texture)
    {
        if (out_texture != nullptr) {
            *out_texture = LunaEditorTextureView{.texture_id = 0x1234u, .width = 320u, .height = 180u};
            return 1;
        }
        return 0;
    }

    static void editorCameraPosition(void*, LunaEditorVec3* out_position)
    {
        if (out_position != nullptr) {
            *out_position = LunaEditorVec3{.x = 1.0f, .y = 2.0f, .z = 3.0f};
        }
    }

    static int gizmoOperationName(void*, char* out_value, size_t out_value_size)
    {
        copyToBuffer(out_value, out_value_size, "Translate");
        return 1;
    }

    static int gizmoModeName(void*, char* out_value, size_t out_value_size)
    {
        copyToBuffer(out_value, out_value_size, "Local");
        return 1;
    }

    static int pickDebugVisualizationEnabled(void*) { return 0; }
    static void setPickDebugVisualizationEnabled(void*, int) {}
    static int editorGridEnabled(void*) { return 1; }
    static void setEditorGridEnabled(void*, int) {}
    static uint64_t defaultSceneViewport(void*) { return 1u; }

    static uint64_t createSceneViewport(void* api_user_data, const char*)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        host->created_viewport_id = host->next_viewport_id++;
        return host->created_viewport_id;
    }

    static void destroySceneViewport(void* api_user_data, uint64_t viewport_id)
    {
        if (TemplateHost* host = self(api_user_data); host != nullptr && host->created_viewport_id == viewport_id) {
            host->created_viewport_id = 0u;
        }
    }

    static int isSceneViewportValid(void* api_user_data, uint64_t viewport_id)
    {
        TemplateHost* host = self(api_user_data);
        return host != nullptr && (viewport_id == 1u || viewport_id == host->created_viewport_id) ? 1 : 0;
    }

    static int syncSceneViewportEx(void* api_user_data,
                                   uint64_t viewport_id,
                                   uint32_t width,
                                   uint32_t height,
                                   LunaEditorViewportPresentation* out_presentation)
    {
        return isSceneViewportValid(api_user_data, viewport_id) != 0
                   ? syncSceneViewport(api_user_data, width, height, out_presentation)
                   : 0;
    }

    static int sceneTextureViewEx(void* api_user_data, uint64_t viewport_id, LunaEditorTextureView* out_texture)
    {
        return isSceneViewportValid(api_user_data, viewport_id) != 0 ? sceneTextureView(api_user_data, out_texture) : 0;
    }

    static int isRuntimeViewportEnabled(void*) { return 0; }
    static int isRuntimeViewportRequested(void*) { return 0; }
    static void setRuntimeViewportRequested(void*, int) {}
    static size_t runtimeEntityCount(void*) { return 17u; }
};

std::filesystem::path absolutePathFromArg(const char* value)
{
    return std::filesystem::absolute(std::filesystem::path(value)).lexically_normal();
}

} // namespace

int main(int argc, char** argv)
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    if (argc != 2) {
        context.expect(false, "expected one plugin binary path argument");
        luna::Logger::shutdown();
        return context.result();
    }

    const std::filesystem::path plugin_binary = absolutePathFromArg(argv[1]);
    context.expect(std::filesystem::exists(plugin_binary), "SDK template plugin binary should exist");

    std::shared_ptr<luna::DynamicLibrary> library = luna::DynamicLibrary::load(plugin_binary);
    if (!context.expect(library != nullptr, "SDK template plugin binary should load")) {
        luna::Logger::shutdown();
        return context.result();
    }

    auto* create_plugin =
        reinterpret_cast<LunaCreateEditorPluginFn>(library->findSymbol(LUNA_EDITOR_CREATE_PLUGIN_SYMBOL));
    if (!context.expect(create_plugin != nullptr, "SDK template should export LunaCreateEditorPlugin")) {
        luna::Logger::shutdown();
        return context.result();
    }

    TemplateHost host;
    LunaEditorPluginApi plugin_api{};
    plugin_api.struct_size = sizeof(LunaEditorPluginApi);
    plugin_api.api_version = LUNA_EDITOR_PLUGIN_API_VERSION;

    context.expect(create_plugin(LUNA_EDITOR_HOST_API_VERSION, &host.api, &plugin_api) != 0,
                   "SDK template create function should fill plugin API");
    context.expect(plugin_api.struct_size == sizeof(LunaEditorPluginApi), "SDK template plugin API size should match");
    context.expect(plugin_api.api_version == LUNA_EDITOR_PLUGIN_API_VERSION, "SDK template plugin API version should be v1");
    context.expect(plugin_api.plugin_id != nullptr && std::string_view(plugin_api.plugin_id) == kExpectedPluginId,
                   "SDK template plugin id should match manifest");
    context.expect(plugin_api.on_load != nullptr && plugin_api.on_unload != nullptr,
                   "SDK template should provide load/unload callbacks");

    if (plugin_api.on_load != nullptr) {
        context.expect(plugin_api.on_load(plugin_api.plugin_user_data, &host.api) != 0,
                       "SDK template on_load should succeed");
    }

    context.expect(host.commands.contains(kExpectedCommandId), "SDK template should register its command");
    context.expect(host.windows.contains(kExpectedWindowId), "SDK template should register its window");
    context.expect(!host.menus.empty(), "SDK template should register a menu item");
    context.expect(host.windows[kExpectedWindowId].open, "SDK template window should start open");

    context.expect(host.drawWindow(kExpectedWindowId), "SDK template registered window should draw");
    context.expect(host.text_count > 0, "SDK template draw should use UI text");
    context.expect(host.plugin_asset_read_text_count > 0, "SDK template should read plugin assets through host API");
    context.expect(host.project_info_count > 0, "SDK template should read project info through host API");
    context.expect(host.list_assets_count > 0, "SDK template should enumerate project assets through host API");
    context.expect(host.image_count > 0, "SDK template should draw a scene viewport texture");
    context.expect(host.created_viewport_id != 0u, "SDK template should create an independent scene viewport");

    host.next_button_pressed = true;
    context.expect(host.drawWindow(kExpectedWindowId), "SDK template button draw should run");
    context.expect(host.created_entity_count == 1, "SDK template button should create one entity");
    context.expect(host.selected_entity_id >= 100u, "SDK template should select its created entity");

    if (plugin_api.on_unload != nullptr) {
        plugin_api.on_unload(plugin_api.plugin_user_data, &host.api);
    }
    context.expect(host.commands.empty(), "SDK template unload should unregister commands");
    context.expect(host.windows.empty(), "SDK template unload should unregister windows");
    context.expect(host.menus.empty(), "SDK template unload should remove menu items");
    context.expect(host.created_viewport_id == 0u, "SDK template unload should destroy its viewport");

    luna::Logger::shutdown();
    return context.result();
}
