#include "Core/Log.h"
#include "EditorApi/EditorAssetService.h"
#include "EditorApi/EditorCommandService.h"
#include "EditorApi/EditorHistoryService.h"
#include "EditorApi/EditorMaterialService.h"
#include "EditorApi/EditorMenuService.h"
#include "EditorApi/EditorNativePluginApi.h"
#include "EditorApi/EditorPluginAssetService.h"
#include "EditorApi/EditorProjectService.h"
#include "EditorApi/EditorRenderingService.h"
#include "EditorApi/EditorRuntimeViewportService.h"
#include "EditorApi/EditorSceneService.h"
#include "EditorApi/EditorScriptPluginService.h"
#include "EditorApi/EditorScriptService.h"
#include "EditorApi/EditorSelectionService.h"
#include "EditorApi/EditorSettingsService.h"
#include "EditorApi/EditorShortcutService.h"
#include "EditorApi/EditorUi.h"
#include "EditorApi/EditorViewportService.h"
#include "EditorApi/EditorWindowService.h"
#include "Platform/Common/DynamicLibrary.h"
#include "Shell/EditorPluginManifest.h"
#include "Shell/EditorPluginManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
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
    int section_count{};
    int combo_count{};
    int tree_count{};
    int drag_drop_source_count{};
    int drag_drop_target_count{};
    int tooltip_count{};
    int heading_count{};
    int key_value_count{};
    int badge_count{};
    int metric_count{};
    int asset_field_count{};
    int asset_drop_count{};
    int panel_depth{};
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
            .bullet_text = &text,
            .separator = &separator,
            .separator_text = &separatorText,
            .same_line = &sameLine,
            .spacing = &spacing,
            .indent = &indent,
            .unindent = &unindent,
            .begin_disabled = &beginDisabled,
            .end_disabled = &endDisabled,
            .set_next_item_width = &setNextItemWidth,
            .content_region_avail = &contentRegionAvail,
            .window_framebuffer_scale = &windowFramebufferScale,
            .button = &button,
            .checkbox = &checkbox,
            .slider_int = &sliderInt,
            .slider_float = &sliderFloat,
            .drag_int = &dragInt,
            .drag_float = &dragFloat,
            .drag_float3 = &dragFloat3,
            .input_text = &inputText,
            .input_text_with_hint = &inputTextWithHint,
            .tree_node = &treeNode,
            .tree_node_ex = &treeNodeEx,
            .tree_pop = &treePop,
            .begin_combo = &beginCombo,
            .end_combo = &endCombo,
            .selectable = &selectable,
            .set_item_default_focus = &setItemDefaultFocus,
            .image = &image,
            .is_item_hovered = &isItemHovered,
            .is_item_clicked = &isItemClicked,
            .is_item_double_clicked = &isItemDoubleClicked,
            .is_item_deactivated_after_edit = &isItemDeactivatedAfterEdit,
            .set_tooltip = &setTooltip,
            .invisible_button = &invisibleButton,
            .begin_section = &beginSection,
            .end_section = &endSection,
            .begin_menu = &beginMenu,
            .end_menu = &endMenu,
            .menu_item = &menuItem,
            .open_popup = &openPopup,
            .begin_popup = &beginPopup,
            .begin_popup_context_item = &beginPopupContextItem,
            .close_current_popup = &closeCurrentPopup,
            .end_popup = &endPopup,
            .begin_drag_drop_source = &beginDragDropSource,
            .set_drag_drop_payload = &setDragDropPayload,
            .end_drag_drop_source = &endDragDropSource,
            .begin_drag_drop_target = &beginDragDropTarget,
            .accept_drag_drop_payload = &acceptDragDropPayload,
            .end_drag_drop_target = &endDragDropTarget,
            .scale = &scale,
            .scaled = &scaled,
            .begin_table = &beginTable,
            .end_table = &endTable,
            .table_setup_column = &tableSetupColumn,
            .table_headers_row = &tableHeadersRow,
            .table_next_row = &tableNextRow,
            .table_next_column = &tableNextColumn,
            .heading = &heading,
            .key_value = &keyValue,
            .badge = &badge,
            .metric = &metric,
            .empty_state = &emptyState,
            .begin_panel = &beginPanel,
            .end_panel = &endPanel,
            .asset_field = &assetField,
            .accept_asset_drag_drop_payload = &acceptAssetDragDropPayload,
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
    static void sameLine(void*) {}
    static void spacing(void*) {}
    static void indent(void*, float) {}
    static void unindent(void*, float) {}
    static void beginDisabled(void*) {}
    static void endDisabled(void*) {}
    static void setNextItemWidth(void*, float) {}

    static void contentRegionAvail(void*, LunaEditorVec2* out_value)
    {
        if (out_value != nullptr) {
            *out_value = LunaEditorVec2{.x = 320.0f, .y = 180.0f};
        }
    }

    static void windowFramebufferScale(void*, LunaEditorVec2* out_value)
    {
        if (out_value != nullptr) {
            *out_value = LunaEditorVec2{.x = 1.0f, .y = 1.0f};
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

    static int sliderInt(void*, const char*, int*, int, int)
    {
        return 0;
    }

    static int sliderFloat(void*, const char*, float*, float, float, const char*)
    {
        return 0;
    }

    static int dragInt(void*, const char*, int*, float, int, int)
    {
        return 0;
    }

    static int dragFloat(void*, const char*, float*, float, float, float, const char*)
    {
        return 0;
    }

    static int dragFloat3(void*, const char*, LunaEditorVec3*, float, float, float, const char*)
    {
        return 0;
    }

    static int inputText(void*, const char*, char*, size_t)
    {
        return 0;
    }

    static int inputTextWithHint(void*, const char*, const char*, char*, size_t)
    {
        return 0;
    }

    static int treeNode(void* api_user_data, const char*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->tree_count;
        }
        return 1;
    }

    static int treeNodeEx(void* api_user_data, const char*, const char*, uint32_t)
    {
        return treeNode(api_user_data, nullptr);
    }

    static void treePop(void*) {}

    static int beginCombo(void* api_user_data, const char*, const char*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->combo_count;
        }
        return 1;
    }

    static void endCombo(void*) {}

    static int selectable(void*, const char*, int)
    {
        return 0;
    }

    static void setItemDefaultFocus(void*) {}

    static int image(void* api_user_data, const LunaEditorTextureView* texture, const LunaEditorVec2*)
    {
        TemplateHost* host = self(api_user_data);
        if (host == nullptr || texture == nullptr || texture->texture_id == 0u) {
            return 0;
        }
        ++host->image_count;
        return 1;
    }

    static int isItemHovered(void*)
    {
        return 1;
    }

    static int isItemClicked(void*, int)
    {
        return 0;
    }

    static int isItemDoubleClicked(void*, int)
    {
        return 0;
    }

    static int isItemDeactivatedAfterEdit(void*)
    {
        return 0;
    }

    static void setTooltip(void* api_user_data, const char*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->tooltip_count;
        }
    }

    static int invisibleButton(void*, const char*, const LunaEditorVec2*)
    {
        return 0;
    }

    static int beginSection(void* api_user_data, const char*, const char*, int)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->section_count;
        }
        return 1;
    }

    static void endSection(void*) {}

    static int beginMenu(void*, const char*, int)
    {
        return 1;
    }

    static void endMenu(void*) {}

    static int menuItem(void*, const char*, int, int)
    {
        return 0;
    }

    static void openPopup(void*, const char*) {}

    static int beginPopup(void*, const char*)
    {
        return 0;
    }

    static int beginPopupContextItem(void*, const char*, int)
    {
        return 0;
    }

    static void closeCurrentPopup(void*) {}
    static void endPopup(void*) {}

    static int beginDragDropSource(void* api_user_data)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->drag_drop_source_count;
        }
        return 1;
    }

    static int setDragDropPayload(void*, const char*, const void*, size_t)
    {
        return 1;
    }

    static void endDragDropSource(void*) {}

    static int beginDragDropTarget(void* api_user_data)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->drag_drop_target_count;
        }
        return 1;
    }

    static int acceptDragDropPayload(void*, const char*, void* out_data, size_t size)
    {
        if (out_data != nullptr && size == sizeof(uint64_t)) {
            uint64_t value = 0u;
            std::memcpy(out_data, &value, sizeof(value));
        }
        return 1;
    }

    static int acceptAssetDragDropPayload(void* api_user_data,
                                          LunaEditorAssetDropPayload* out_payload,
                                          const uint32_t*,
                                          size_t)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->asset_drop_count;
        }
        if (out_payload != nullptr) {
            out_payload->handle = 42u;
            out_payload->type = LunaEditorAssetType_Texture;
        }
        return 1;
    }

    static void endDragDropTarget(void*) {}

    static float scale(void*, float value)
    {
        return value;
    }

    static void scaled(void*, const LunaEditorVec2* value, LunaEditorVec2* out_value)
    {
        if (value != nullptr && out_value != nullptr) {
            *out_value = *value;
        }
    }

    static int beginTable(void*, const char*, int, uint32_t, const LunaEditorVec2*)
    {
        return 1;
    }

    static void endTable(void*) {}
    static void tableSetupColumn(void*, const char*, uint32_t, float) {}
    static void tableHeadersRow(void*) {}
    static void tableNextRow(void*) {}

    static int tableNextColumn(void*)
    {
        return 1;
    }

    static void heading(void* api_user_data, const char*, const char*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->heading_count;
            ++host->text_count;
        }
    }

    static void keyValue(void* api_user_data, const char*, const char*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->key_value_count;
            ++host->text_count;
        }
    }

    static void badge(void* api_user_data, const char*, uint32_t)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->badge_count;
            ++host->text_count;
        }
    }

    static void metric(void* api_user_data, const char*, const char*, const char*, uint32_t, const LunaEditorVec2*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->metric_count;
            ++host->text_count;
        }
    }

    static void emptyState(void* api_user_data, const char*, const char*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->text_count;
        }
    }

    static void beginPanel(void* api_user_data, const char*, const LunaEditorVec2*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->panel_depth;
        }
    }

    static void endPanel(void* api_user_data)
    {
        if (TemplateHost* host = self(api_user_data)) {
            --host->panel_depth;
        }
    }

    static int assetField(void* api_user_data, const char*, const char*, const char*, uint32_t, const LunaEditorVec2*)
    {
        if (TemplateHost* host = self(api_user_data)) {
            ++host->asset_field_count;
        }
        return 0;
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

class TemplateManagerUi final : public luna::editor::Ui {
public:
    bool beginWindow(std::string_view, std::string_view, bool*, luna::editor::WindowFlags) override { return true; }
    void endWindow() override {}
    void text(std::string_view) override { ++text_count; }
    void textDisabled(std::string_view) override { ++text_count; }
    void textWrapped(std::string_view) override { ++text_count; }
    void bulletText(std::string_view) override { ++text_count; }
    void separator() override {}
    void separatorText(std::string_view) override {}
    void sameLine() override {}
    void spacing() override {}
    void indent(float = 0.0f) override {}
    void unindent(float = 0.0f) override {}
    void beginDisabled() override {}
    void endDisabled() override {}
    void setNextItemWidth(float) override {}
    [[nodiscard]] luna::editor::Vec2 contentRegionAvail() const noexcept override { return {.x = 320.0f, .y = 180.0f}; }
    [[nodiscard]] luna::editor::Vec2 windowFramebufferScale() const noexcept override { return {.x = 1.0f, .y = 1.0f}; }
    void heading(std::string_view, std::string_view = {}) override
    {
        ++heading_count;
        ++text_count;
    }
    void keyValue(std::string_view, std::string_view) override
    {
        ++key_value_count;
        ++text_count;
    }
    void badge(std::string_view, luna::editor::StatusVariant = luna::editor::StatusVariant::Neutral) override
    {
        ++badge_count;
        ++text_count;
    }
    void metric(std::string_view, std::string_view, std::string_view = {}, luna::editor::StatusVariant = luna::editor::StatusVariant::Neutral, luna::editor::Vec2 = {}) override
    {
        ++metric_count;
        ++text_count;
    }
    void emptyState(std::string_view, std::string_view = {}) override { ++text_count; }
    void beginPanel(std::string_view, luna::editor::Vec2 = {}) override { ++panel_depth; }
    void endPanel() override { --panel_depth; }
    bool button(std::string_view, luna::editor::Vec2 = {}, luna::editor::ButtonVariant = luna::editor::ButtonVariant::Default) override
    {
        const bool pressed = next_button_pressed;
        next_button_pressed = false;
        ++button_count;
        return pressed;
    }
    bool checkbox(std::string_view, bool&) override { return false; }
    bool colorEdit3(std::string_view, luna::editor::Vec3&) override { return false; }
    bool sliderInt(std::string_view, int&, int, int) override
    {
        ++slider_count;
        return false;
    }
    bool sliderFloat(std::string_view, float&, float, float, std::string_view = "%.3f") override { return false; }
    bool dragFloat3(std::string_view, luna::editor::Vec3&, float, float, float, std::string_view = "%.3f") override
    {
        ++drag_float3_count;
        return false;
    }
    bool dragInt(std::string_view, int&, float, int, int) override { return false; }
    bool dragFloat(std::string_view, float&, float, float, float, std::string_view = "%.3f") override { return false; }
    bool inputText(std::string_view, std::string&, std::size_t = 256) override { return false; }
    bool inputTextWithHint(std::string_view, std::string_view, std::string&, std::size_t = 256) override { return false; }
    bool colorEdit4(std::string_view, luna::editor::Vec4&) override { return false; }
    bool treeNode(std::string_view) override
    {
        ++tree_count;
        return true;
    }
    void treePop() override {}
    bool beginCombo(std::string_view, std::string_view) override
    {
        ++combo_count;
        return true;
    }
    void endCombo() override {}
    bool selectable(std::string_view, bool = false) override { return false; }
    void setItemDefaultFocus() override {}
    bool image(const luna::editor::TextureView& texture, luna::editor::Vec2) override
    {
        if (texture.valid()) {
            ++image_count;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool isItemHovered() const noexcept override { return true; }
    [[nodiscard]] bool isItemClicked(luna::editor::MouseButton = luna::editor::MouseButton::Left) const noexcept override { return false; }
    [[nodiscard]] bool isItemDoubleClicked(luna::editor::MouseButton = luna::editor::MouseButton::Left) const noexcept override { return false; }
    [[nodiscard]] bool isItemDeactivatedAfterEdit() const noexcept override { return false; }
    void setTooltip(std::string_view) override { ++tooltip_count; }
    bool invisibleButton(std::string_view, luna::editor::Vec2) override { return false; }
    bool treeNodeEx(std::string_view, std::string_view, luna::editor::TreeNodeFlags) override
    {
        ++tree_count;
        return true;
    }
    bool beginSection(std::string_view, std::string_view, bool = true) override
    {
        ++section_count;
        return true;
    }
    void endSection() override {}
    bool beginMenu(std::string_view, bool = true) override { return true; }
    void endMenu() override {}
    bool menuItem(std::string_view, bool = false, bool = true) override { return false; }
    void openPopup(std::string_view) override {}
    bool beginPopup(std::string_view) override { return false; }
    bool beginPopupContextItem(std::string_view = {}, luna::editor::MouseButton = luna::editor::MouseButton::Right) override { return false; }
    void closeCurrentPopup() override {}
    void endPopup() override {}
    bool beginDragDropSource() override
    {
        ++drag_drop_source_count;
        return true;
    }
    bool setDragDropPayload(std::string_view, const void*, std::size_t) override { return true; }
    void endDragDropSource() override {}
    bool beginDragDropTarget() override
    {
        ++drag_drop_target_count;
        return true;
    }
    bool acceptDragDropPayload(std::string_view, void*, std::size_t) override { return true; }
    bool acceptAssetDragDropPayload(luna::editor::AssetDropPayload& out_payload,
                                    const luna::AssetType* accepted_types,
                                    std::size_t accepted_type_count) override
    {
        ++asset_drop_count;
        const luna::AssetType dropped_type = luna::AssetType::Texture;
        bool accepted = accepted_type_count == 0u;
        for (std::size_t index = 0u; index < accepted_type_count && !accepted; ++index) {
            accepted = accepted_types != nullptr && accepted_types[index] == dropped_type;
        }
        if (!accepted) {
            return false;
        }
        out_payload = luna::editor::AssetDropPayload{
            .handle = luna::AssetHandle(42u),
            .type = dropped_type,
        };
        return true;
    }
    void endDragDropTarget() override {}
    [[nodiscard]] float scale(float value) const noexcept override { return value; }
    [[nodiscard]] luna::editor::Vec2 scaled(luna::editor::Vec2 value) const noexcept override { return value; }
    bool beginTable(std::string_view, int, luna::editor::TableFlags = static_cast<luna::editor::TableFlags>(luna::editor::TableFlag::None), luna::editor::Vec2 = {}) override { return true; }
    void endTable() override {}
    void tableSetupColumn(std::string_view, luna::editor::TableColumnFlags = static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::None), float = 0.0f) override {}
    void tableHeadersRow() override {}
    void tableNextRow() override {}
    bool tableNextColumn() override { return true; }
    bool assetField(std::string_view, std::string_view, std::string_view = {}, luna::editor::StatusVariant = luna::editor::StatusVariant::Neutral, luna::editor::Vec2 = {}) override
    {
        ++asset_field_count;
        return false;
    }

    bool next_button_pressed{};
    int text_count{};
    int button_count{};
    int slider_count{};
    int drag_float3_count{};
    int section_count{};
    int combo_count{};
    int tree_count{};
    int drag_drop_source_count{};
    int drag_drop_target_count{};
    int image_count{};
    int tooltip_count{};
    int heading_count{};
    int key_value_count{};
    int badge_count{};
    int metric_count{};
    int asset_field_count{};
    int asset_drop_count{};
    int panel_depth{};
};

class TemplateManagerCommandService final : public luna::editor::CommandService {
public:
    explicit TemplateManagerCommandService(luna::editor::Host& host)
        : m_host(host)
    {}

    bool registerCommand(luna::editor::CommandDescriptor descriptor) override
    {
        if (descriptor.id.empty() || !descriptor.execute) {
            return false;
        }
        commands[descriptor.id] = std::move(descriptor);
        return true;
    }
    void unregisterCommand(std::string_view id) override { commands.erase(std::string(id)); }
    bool execute(std::string_view id) override { return execute(id, std::nullopt); }
    bool execute(std::string_view id, luna::editor::CommandSubject subject) override
    {
        const auto it = commands.find(std::string(id));
        if (it == commands.end() || !it->second.execute) {
            return false;
        }
        if (it->second.can_execute && !it->second.can_execute(m_host)) {
            return false;
        }
        subjects[it->first] = std::move(subject);
        it->second.execute(m_host);
        return true;
    }
    luna::editor::CommandSubject takeSubject(std::string_view id) override
    {
        const auto it = subjects.find(std::string(id));
        if (it == subjects.end()) {
            return std::nullopt;
        }
        luna::editor::CommandSubject subject = std::move(it->second);
        subjects.erase(it);
        return subject;
    }
    bool canExecute(std::string_view id) const override
    {
        const auto it = commands.find(std::string(id));
        return it != commands.end() && (!it->second.can_execute || it->second.can_execute(m_host));
    }
    bool isChecked(std::string_view id) const override
    {
        const auto it = commands.find(std::string(id));
        return it != commands.end() && it->second.is_checked && it->second.is_checked(m_host);
    }
    void removeOwner(std::string_view owner_id)
    {
        for (auto it = commands.begin(); it != commands.end();) {
            if (it->second.owner_id == owner_id) {
                it = commands.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::unordered_map<std::string, luna::editor::CommandDescriptor> commands;

private:
    luna::editor::Host& m_host;
    std::unordered_map<std::string, luna::editor::CommandSubject> subjects;
};

class TemplateManagerWindowService final : public luna::editor::WindowService {
public:
    bool registerWindow(luna::editor::WindowDescriptor descriptor) override
    {
        if (descriptor.id.empty() || !descriptor.draw) {
            return false;
        }
        open[descriptor.id] = descriptor.default_open;
        windows[descriptor.id] = std::move(descriptor);
        return true;
    }
    bool registerDockspaceWindow(luna::editor::DockspaceWindowDescriptor descriptor) override
    {
        if (descriptor.id.empty()) {
            return false;
        }
        open[descriptor.id] = descriptor.default_open;
        dockspaces[descriptor.id] = std::move(descriptor);
        return true;
    }
    void unregisterWindow(std::string_view id) override
    {
        windows.erase(std::string(id));
        dockspaces.erase(std::string(id));
        open.erase(std::string(id));
    }
    bool isWindowOpen(std::string_view id) const override
    {
        const auto it = open.find(std::string(id));
        return it != open.end() && it->second;
    }
    void setWindowOpen(std::string_view id, bool value) override
    {
        const std::string key(id);
        if (windows.contains(key) || dockspaces.contains(key)) {
            open[key] = value;
        }
    }
    bool drawWindow(std::string_view id, luna::editor::Host& host, luna::editor::Ui& ui)
    {
        const auto it = windows.find(std::string(id));
        if (it == windows.end() || !it->second.draw) {
            return false;
        }
        luna::editor::WindowDrawContext context(host, ui);
        it->second.draw(context);
        return true;
    }
    void removeOwner(std::string_view owner_id)
    {
        for (auto it = windows.begin(); it != windows.end();) {
            if (it->second.owner_id == owner_id) {
                open.erase(it->first);
                it = windows.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = dockspaces.begin(); it != dockspaces.end();) {
            if (it->second.owner_id == owner_id) {
                open.erase(it->first);
                it = dockspaces.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::unordered_map<std::string, luna::editor::WindowDescriptor> windows;
    std::unordered_map<std::string, luna::editor::DockspaceWindowDescriptor> dockspaces;
    std::unordered_map<std::string, bool> open;
};

class TemplateManagerMenuService final : public luna::editor::MenuService {
public:
    bool addMenuItem(luna::editor::MenuItemDescriptor descriptor) override
    {
        if (descriptor.menu_path.empty() || descriptor.command_id.empty()) {
            return false;
        }
        menus.push_back(std::move(descriptor));
        return true;
    }
    void removeMenuItem(std::string_view menu_path, std::string_view command_id) override
    {
        menus.erase(std::remove_if(menus.begin(), menus.end(), [&](const auto& item) {
                        return item.menu_path == menu_path && item.command_id == command_id;
                    }),
                    menus.end());
    }
    void removeMenuItemsForCommand(std::string_view command_id) override
    {
        menus.erase(std::remove_if(menus.begin(), menus.end(), [&](const auto& item) {
                        return item.command_id == command_id;
                    }),
                    menus.end());
    }
    void removeOwner(std::string_view owner_id)
    {
        menus.erase(std::remove_if(menus.begin(), menus.end(), [&](const auto& item) {
                        return item.owner_id == owner_id;
                    }),
                    menus.end());
    }

    std::vector<luna::editor::MenuItemDescriptor> menus;
};

class TemplateManagerAssetService final : public luna::editor::AssetService {
public:
    TemplateManagerAssetService()
    {
        asset.handle = luna::AssetHandle(42u);
        asset.type = luna::AssetType::Texture;
        asset.label = "SDK Texture";
        asset.detail = "SDK template manager asset";
        asset.exists = true;
        asset.project_path = "Textures/sdk.png";
        asset.absolute_path = "F:/SdkTemplateProject/Assets/Textures/sdk.png";
    }

    luna::editor::AssetInfo describeAsset(luna::AssetHandle handle) const override
    {
        return handle == asset.handle ? asset : luna::editor::AssetInfo{};
    }
    std::optional<luna::editor::AssetInfo> assetInfo(luna::AssetHandle handle) const override
    {
        return handle == asset.handle ? std::optional<luna::editor::AssetInfo>(asset) : std::nullopt;
    }
    std::optional<luna::editor::AssetInfo> assetInfoByPath(const std::filesystem::path& path) const override
    {
        return path == asset.project_path ? std::optional<luna::editor::AssetInfo>(asset) : std::nullopt;
    }
    std::vector<luna::editor::AssetInfo> listAssets(luna::AssetType type_filter, bool) const override
    {
        ++list_count;
        if (type_filter == luna::AssetType::None || type_filter == asset.type) {
            return {asset};
        }
        return {};
    }
    std::vector<luna::editor::AssetInfo> builtinAssets(luna::AssetType) const override { return {}; }
    bool assetExists(luna::AssetHandle handle) const override { return handle == asset.handle; }
    bool assetPathExists(const std::filesystem::path& path) const override { return path == asset.project_path; }
    luna::AssetHandle findAssetHandleByPath(const std::filesystem::path& path) const override
    {
        return path == asset.project_path ? asset.handle : luna::AssetHandle(0u);
    }
    std::optional<std::filesystem::path> assetsRootPath() const override { return std::filesystem::path("F:/SdkTemplateProject/Assets"); }
    std::optional<std::filesystem::path> resolveProjectAssetPath(const std::filesystem::path& path) const override
    {
        return std::filesystem::path("F:/SdkTemplateProject/Assets") / path;
    }
    std::optional<std::filesystem::path> makeProjectRelativeAssetPath(const std::filesystem::path& path) const override
    {
        return path.filename();
    }
    luna::editor::AssetRefreshResult refreshAssets() override { return {.success = true, .project_loaded = true, .revision = revision}; }
    uint64_t assetRevision() const noexcept override { return revision; }
    bool isAssetLoading(luna::AssetHandle) const override { return false; }
    bool acceptsAssetType(luna::AssetType type, const luna::AssetType* accepted_types, std::size_t accepted_type_count) const override
    {
        if (accepted_types == nullptr || accepted_type_count == 0u) {
            return true;
        }
        return std::find(accepted_types, accepted_types + accepted_type_count, type) != accepted_types + accepted_type_count;
    }
    std::optional<std::size_t> meshSubmeshCount(luna::AssetHandle) const override { return 1u; }
    bool beginAssetDragDropSource(luna::AssetHandle, std::string_view = {}) override { return false; }

    luna::editor::AssetInfo asset;
    uint64_t revision{9u};
    mutable int list_count{};
};

class TemplateManagerMaterialService final : public luna::editor::MaterialService {
public:
    luna::editor::MaterialCreateResult createMaterial(const luna::editor::MaterialCreateRequest&) override { return {}; }
    bool canEditMaterial(luna::AssetHandle) const override { return false; }
    std::optional<luna::editor::MaterialDocument> readMaterial(luna::AssetHandle) override { return std::nullopt; }
    bool setMaterialTextures(luna::AssetHandle, const luna::editor::MaterialTextureSet&) override { return false; }
    bool setMaterialSurface(luna::AssetHandle, const luna::editor::MaterialSurfaceProperties&) override { return false; }
    luna::editor::MetallicRoughnessSynthesisResult
        synthesizeMetallicRoughness(const luna::editor::MetallicRoughnessSynthesisRequest&) override
    {
        return {};
    }
    luna::editor::MaterialEditResult saveMaterial(luna::AssetHandle) override { return {}; }
    luna::editor::MaterialEditResult revertMaterial(luna::AssetHandle) override { return {}; }
    bool isMaterialDirty(luna::AssetHandle) const override { return false; }
};

class TemplateManagerPluginAssetService final : public luna::editor::PluginAssetService {
public:
    void registerPlugin(std::string_view plugin_id, const std::filesystem::path& root_path)
    {
        roots[std::string(plugin_id)] = root_path;
    }
    void unregisterPlugin(std::string_view plugin_id)
    {
        roots.erase(std::string(plugin_id));
    }
    [[nodiscard]] std::optional<std::filesystem::path> pluginRootPath(std::string_view plugin_id) const override
    {
        const auto it = roots.find(std::string(plugin_id));
        return it != roots.end() ? std::optional<std::filesystem::path>(it->second) : std::nullopt;
    }
    [[nodiscard]] std::optional<std::filesystem::path> assetRootPath(std::string_view plugin_id) const override
    {
        const auto root = pluginRootPath(plugin_id);
        return root ? std::optional<std::filesystem::path>(*root / "assets") : std::nullopt;
    }
    [[nodiscard]] std::optional<std::filesystem::path> resolvePath(std::string_view plugin_id, const std::filesystem::path& path) const override
    {
        const auto root = assetRootPath(plugin_id);
        return root ? std::optional<std::filesystem::path>(*root / path) : std::nullopt;
    }
    [[nodiscard]] bool exists(std::string_view plugin_id, const std::filesystem::path& path) const override
    {
        return resolvePath(plugin_id, path).has_value();
    }
    [[nodiscard]] std::optional<std::string> readText(std::string_view plugin_id, const std::filesystem::path& path) const override
    {
        if (!exists(plugin_id, path)) {
            return std::nullopt;
        }
        ++read_text_count;
        return std::string("SDK template manager asset text");
    }
    [[nodiscard]] luna::editor::PluginAssetBytes readBytes(std::string_view plugin_id, const std::filesystem::path& path) const override
    {
        return exists(plugin_id, path) ? luna::editor::PluginAssetBytes{.data = {1u, 2u, 3u}}
                                      : luna::editor::PluginAssetBytes{};
    }
    [[nodiscard]] luna::editor::TextureView texture(std::string_view, const std::filesystem::path&) override
    {
        return {.id = 0x99u, .size = {.x = 16u, .y = 16u}, .y_flip = false};
    }

    std::unordered_map<std::string, std::filesystem::path> roots;
    mutable int read_text_count{};
};

class TemplateManagerProjectService final : public luna::editor::ProjectService {
public:
    [[nodiscard]] bool hasProjectLoaded() const override { return true; }
    [[nodiscard]] std::optional<std::filesystem::path> projectRootPath() const override { return std::filesystem::path("F:/SdkTemplateProject"); }
    [[nodiscard]] std::optional<luna::ProjectInfo> projectInfo() const override
    {
        ++info_count;
        luna::ProjectInfo info{};
        info.Name = "SDK Template Project";
        info.Version = "0.1.0";
        info.AssetsPath = "Assets";
        return info;
    }
    void setProjectInfo(const luna::ProjectInfo&) override {}
    bool saveProject() override { return true; }

    mutable int info_count{};
};

class TemplateManagerSceneService final : public luna::editor::SceneService {
public:
    TemplateManagerSceneService()
    {
        luna::editor::SceneEntityDetails root{};
        root.id = luna::editor::EntityId(1u);
        root.name = "Root";
        root.components.transform = true;
        root.transform.scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f};
        entities[root.id] = root;
    }

    std::string sceneLabel() const override { return "SDK Template Scene"; }
    size_t entityCount() const override { return entities.size(); }
    bool canEditScene() const noexcept override { return true; }
    bool openSceneFile(const std::filesystem::path&) override { return true; }
    std::vector<luna::editor::SceneEntityInfo> entityHierarchy() const override
    {
        std::vector<luna::editor::SceneEntityInfo> result;
        for (const auto& [id, entity] : entities) {
            result.push_back({.id = id, .parent_id = entity.parent_id, .name = entity.name});
        }
        return result;
    }
    bool entityExists(luna::editor::EntityId entity_id) const noexcept override { return entities.contains(entity_id); }
    std::optional<luna::editor::SceneEntityDetails> entityDetails(luna::editor::EntityId entity_id) const override
    {
        const auto it = entities.find(entity_id);
        return it != entities.end() ? std::optional<luna::editor::SceneEntityDetails>(it->second) : std::nullopt;
    }
    bool isEntityDescendantOf(luna::editor::EntityId, luna::editor::EntityId) const override { return false; }
    luna::SceneEnvironmentSettings sceneEnvironmentSettings() const override { return {}; }
    luna::SceneShadowSettings sceneShadowSettings() const override { return {}; }
    bool setSceneEnvironmentSettings(const luna::SceneEnvironmentSettings&) override { return true; }
    bool setSceneShadowSettings(const luna::SceneShadowSettings&) override { return true; }
    luna::editor::EntityId createEntity(std::string name) override
    {
        const luna::editor::EntityId id(next_entity_id++);
        luna::editor::SceneEntityDetails entity{};
        entity.id = id;
        entity.name = name.empty() ? "Entity" : std::move(name);
        entity.components.transform = true;
        entity.transform.scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f};
        entities[id] = entity;
        ++created_entity_count;
        return id;
    }
    luna::editor::EntityId createEntity(const luna::editor::SceneEntityCreateRequest& request) override { return createEntity(request.name); }
    bool destroyEntity(luna::editor::EntityId entity_id) override { return entities.erase(entity_id) > 0u; }
    bool reparentEntity(luna::editor::EntityId entity_id, luna::editor::EntityId parent_id, bool = true) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.parent_id = parent_id;
        return true;
    }
    bool setEntityName(luna::editor::EntityId entity_id, std::string name) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.name = std::move(name);
        return true;
    }
    bool setEntityTransform(luna::editor::EntityId entity_id, const luna::editor::SceneTransform& transform) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.transform = transform;
        return true;
    }
    bool setCameraComponent(luna::editor::EntityId entity_id, const luna::editor::SceneCameraComponent& value) override
    {
        return setComponent(entity_id, value);
    }
    bool setLightComponent(luna::editor::EntityId entity_id, const luna::editor::SceneLightComponent& value) override
    {
        return setComponent(entity_id, value);
    }
    bool setMeshComponent(luna::editor::EntityId entity_id, const luna::editor::SceneMeshComponent& value) override
    {
        return setComponent(entity_id, value);
    }
    bool setScriptComponent(luna::editor::EntityId entity_id, const luna::editor::SceneScriptComponent& value) override
    {
        return setComponent(entity_id, value);
    }
    bool setScriptProperty(luna::editor::EntityId, std::size_t, std::size_t, const luna::editor::SceneScriptProperty&) override { return false; }
    bool addComponent(luna::editor::EntityId entity_id, luna::editor::SceneComponentKind kind) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        if (kind == luna::editor::SceneComponentKind::Camera) {
            it->second.camera = luna::editor::SceneCameraComponent{};
            it->second.components.camera = true;
        } else if (kind == luna::editor::SceneComponentKind::Light) {
            it->second.light = luna::editor::SceneLightComponent{};
            it->second.components.light = true;
        } else if (kind == luna::editor::SceneComponentKind::Mesh) {
            it->second.mesh = luna::editor::SceneMeshComponent{};
            it->second.components.mesh = true;
        }
        return true;
    }
    bool removeComponent(luna::editor::EntityId, luna::editor::SceneComponentKind) override { return true; }
    bool applyMeshAssetToEntity(luna::editor::EntityId entity_id, luna::AssetHandle mesh_handle) override
    {
        luna::editor::SceneMeshComponent mesh{};
        mesh.mesh_handle = mesh_handle;
        return setMeshComponent(entity_id, mesh);
    }

    template <typename Component> bool setComponent(luna::editor::EntityId entity_id, const Component& value)
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        if constexpr (std::is_same_v<Component, luna::editor::SceneCameraComponent>) {
            it->second.camera = value;
            it->second.components.camera = true;
        } else if constexpr (std::is_same_v<Component, luna::editor::SceneLightComponent>) {
            it->second.light = value;
            it->second.components.light = true;
        } else if constexpr (std::is_same_v<Component, luna::editor::SceneMeshComponent>) {
            it->second.mesh = value;
            it->second.components.mesh = true;
        } else {
            it->second.script = value;
            it->second.components.script = true;
        }
        return true;
    }

    std::unordered_map<luna::editor::EntityId, luna::editor::SceneEntityDetails> entities;
    uint64_t next_entity_id{100u};
    int created_entity_count{};
};

class TemplateManagerSelectionService final : public luna::editor::SelectionService {
public:
    luna::editor::EntityId selectedEntityId() const noexcept override { return selected; }
    void selectEntity(luna::editor::EntityId entity_id) override { selected = entity_id; }
    void clearSelection() override { selected = luna::editor::EntityId(0u); }

    luna::editor::EntityId selected{0u};
};

class TemplateManagerViewportService final : public luna::editor::ViewportService {
public:
    luna::editor::ViewportId defaultSceneViewport() const noexcept override { return luna::editor::kDefaultViewportId; }
    luna::editor::ViewportId createSceneViewport(std::string_view debug_name = {}) override
    {
        return createSceneViewportForOwner(debug_name, {});
    }
    void destroySceneViewport(luna::editor::ViewportId viewport_id) override { viewports.erase(viewport_id); }
    bool isSceneViewportValid(luna::editor::ViewportId viewport_id) const noexcept override
    {
        return viewport_id == luna::editor::kDefaultViewportId || viewports.contains(viewport_id);
    }
    luna::editor::ViewportPresentation syncSceneViewport(luna::editor::ViewportId viewport_id, luna::editor::UVec2 framebuffer_size) override
    {
        if (!isSceneViewportValid(viewport_id)) {
            return {};
        }
        return {
            .scene_texture = luna::editor::TextureView{
                .id = static_cast<luna::editor::TextureHandle>(0x700u + viewport_id),
                .size = framebuffer_size,
                .y_flip = false,
            },
            .framebuffer_size = framebuffer_size,
            .presentable = true,
        };
    }
    luna::editor::TextureView sceneTextureView(luna::editor::ViewportId viewport_id) const override
    {
        if (!isSceneViewportValid(viewport_id)) {
            return {};
        }
        return {.id = static_cast<luna::editor::TextureHandle>(0x700u + viewport_id), .size = {.x = 320u, .y = 180u}, .y_flip = false};
    }
    bool setSceneViewportPreview(luna::editor::ViewportId viewport_id,
                                 const luna::editor::SceneViewportPreviewState&) override
    {
        return isSceneViewportValid(viewport_id);
    }
    void clearSceneViewportPreview(luna::editor::ViewportId) override {}
    luna::editor::ViewportPresentation syncSceneViewport(luna::editor::UVec2 framebuffer_size) override
    {
        return syncSceneViewport(luna::editor::kDefaultViewportId, framebuffer_size);
    }
    luna::editor::TextureView sceneTextureView() const override { return sceneTextureView(luna::editor::kDefaultViewportId); }
    void drawDefaultSceneViewport(luna::editor::Ui&) override {}
    luna::editor::SceneViewportDrawResult drawSceneViewport(luna::editor::Ui&, luna::editor::ViewportId viewport_id, luna::editor::SceneViewportDrawOptions = {}) override
    {
        luna::editor::SceneViewportDrawResult result{};
        result.presentation = syncSceneViewport(viewport_id, {.x = 320u, .y = 180u});
        result.drawn = result.presentation.presentable;
        return result;
    }
    luna::editor::ViewportId createTextureViewport(std::string_view = {}) override
    {
        const luna::editor::ViewportId id = next_viewport_id++;
        texture_viewports[id] = {};
        return id;
    }
    void destroyTextureViewport(luna::editor::ViewportId viewport_id) override { texture_viewports.erase(viewport_id); }
    bool isTextureViewportValid(luna::editor::ViewportId viewport_id) const noexcept override { return texture_viewports.contains(viewport_id); }
    luna::editor::TextureViewportPresentation syncTextureViewport(luna::editor::ViewportId viewport_id, luna::editor::TextureView texture, luna::editor::UVec2 framebuffer_size) override
    {
        if (!isTextureViewportValid(viewport_id)) {
            return {};
        }
        luna::editor::TextureViewportPresentation presentation{.texture = texture, .framebuffer_size = framebuffer_size, .presentable = texture.valid()};
        texture_viewports[viewport_id] = presentation;
        return presentation;
    }
    luna::editor::TextureViewportPresentation textureViewportPresentation(luna::editor::ViewportId viewport_id) const override
    {
        const auto it = texture_viewports.find(viewport_id);
        return it != texture_viewports.end() ? it->second : luna::editor::TextureViewportPresentation{};
    }
    luna::editor::TextureViewportDrawResult drawTextureViewport(luna::editor::Ui&, luna::editor::ViewportId viewport_id, luna::editor::TextureView texture, luna::editor::TextureViewportDrawOptions = {}) override
    {
        luna::editor::TextureViewportDrawResult result{};
        result.presentation = syncTextureViewport(viewport_id, texture, texture.size);
        result.drawn = result.presentation.presentable;
        return result;
    }
    luna::editor::Vec3 editorCameraPosition() const noexcept override { return {.x = 1.0f, .y = 2.0f, .z = 3.0f}; }
    std::string gizmoOperationName() const override { return "Translate"; }
    std::string gizmoModeName() const override { return "Local"; }
    bool pickDebugVisualizationEnabled() const noexcept override { return false; }
    void setPickDebugVisualizationEnabled(bool) override {}
    bool editorGridEnabled() const noexcept override { return true; }
    void setEditorGridEnabled(bool) override {}

    luna::editor::ViewportId createSceneViewportForOwner(std::string_view, std::string_view owner_id)
    {
        const luna::editor::ViewportId id = next_viewport_id++;
        viewports[id] = std::string(owner_id);
        return id;
    }
    void destroyViewportsForOwner(std::string_view owner_id)
    {
        for (auto it = viewports.begin(); it != viewports.end();) {
            if (it->second == owner_id) {
                it = viewports.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::unordered_map<luna::editor::ViewportId, std::string> viewports;
    std::unordered_map<luna::editor::ViewportId, luna::editor::TextureViewportPresentation> texture_viewports;
    luna::editor::ViewportId next_viewport_id{2u};
};

class TemplateManagerRuntimeViewportService final : public luna::editor::RuntimeViewportService {
public:
    bool isRuntimeViewportEnabled() const noexcept override { return false; }
    bool isRuntimeViewportRequested() const noexcept override { return requested; }
    void setRuntimeViewportRequested(bool enabled) override { requested = enabled; }
    size_t runtimeEntityCount() const noexcept override { return 17u; }

    bool requested{};
};

class TemplateManagerHistoryService final : public luna::editor::HistoryService {
public:
    bool canUndo() const noexcept override { return false; }
    bool canRedo() const noexcept override { return false; }
    bool hasOpenTransaction() const noexcept override { return false; }
    bool beginTransaction(std::string) override { return false; }
    bool commitTransaction() override { return false; }
    bool rollbackTransaction() override { return false; }
    bool undo() override { return false; }
    bool redo() override { return false; }
};

class TemplateManagerPluginService final : public luna::editor::PluginService {
public:
    std::vector<luna::editor::PluginInfo> plugins() const override { return {}; }
};

class TemplateManagerShortcutService final : public luna::editor::ShortcutService {
public:
    bool registerShortcut(luna::editor::ShortcutDescriptor) override { return true; }
    void unregisterShortcut(std::string_view) override {}
    std::string shortcutText(std::string_view) const override { return {}; }
    std::string commandShortcutText(std::string_view) const override { return {}; }
};

class TemplateManagerRenderingService final : public luna::editor::RenderingService {
public:
    std::string backendName() const override { return "SDKTest"; }
    luna::editor::RenderingBackendCapabilities backendCapabilities() const override { return {}; }
    luna::editor::RenderGraphProfileSnapshot renderGraphProfile() const override { return {}; }
    bool isRenderGraphProfilingEnabled() const noexcept override { return false; }
    void setRenderGraphProfilingEnabled(bool) override {}
    std::filesystem::path defaultRenderProfileExportPath(std::string_view = {}) const override { return {}; }
    bool exportRenderGraphProfileChromeTraceJson(const luna::editor::RenderGraphProfileSnapshot&, const std::filesystem::path&, std::string* = nullptr) const override { return false; }
    std::vector<luna::editor::RenderFeatureInfo> defaultRenderFeatureInfos() const override { return {}; }
    std::vector<luna::editor::RenderFeatureParameterInfo> defaultRenderFeatureParameters(std::string_view) const override { return {}; }
    bool setDefaultRenderFeatureEnabled(std::string_view, bool) override { return false; }
    bool setDefaultRenderFeatureParameter(std::string_view, std::string_view, const luna::editor::RenderFeatureParameterValue&) override { return false; }
    std::vector<luna::editor::RenderDebugViewModeInfo> renderDebugViewModes() const override { return {}; }
    luna::editor::RenderDebugViewMode renderDebugViewMode() const noexcept override { return luna::editor::RenderDebugViewMode::None; }
    void setRenderDebugViewMode(luna::editor::RenderDebugViewMode) override {}
    float renderDebugVelocityScale() const noexcept override { return 1.0f; }
    void setRenderDebugVelocityScale(float) override {}
    luna::editor::TextureView renderDebugTextureView() const override { return {}; }
    float frameTimeMilliseconds() const noexcept override { return 16.0f; }
    float framesPerSecond() const noexcept override { return 60.0f; }
    luna::editor::UVec2 sceneOutputSize() const noexcept override { return {.x = 1280u, .y = 720u}; }
};

class TemplateManagerScriptPluginService final : public luna::editor::ScriptPluginService {
public:
    void refreshProjectScriptPlugins() override {}
    [[nodiscard]] const std::vector<luna::ScriptPluginCandidate>& getDiscoveredScriptPlugins() const override { return candidates; }
    [[nodiscard]] const std::string& getScriptPluginStatus() const override { return status; }
    [[nodiscard]] const luna::ScriptPluginCandidate* getSelectedScriptPluginCandidate() const override { return nullptr; }
    bool selectScriptPlugin(const luna::ScriptPluginCandidate*) override { return false; }

    std::vector<luna::ScriptPluginCandidate> candidates;
    std::string status;
};

class TemplateManagerScriptService final : public luna::editor::ScriptService {
public:
    [[nodiscard]] luna::editor::ScriptLanguageStatus projectScriptLanguage() const override { return {}; }
    [[nodiscard]] luna::editor::ScriptAssetValidation validateScriptAsset(luna::AssetHandle) const override { return {}; }
    [[nodiscard]] luna::editor::ScriptSchemaSyncResult syncScriptProperties(const luna::editor::SceneScriptEntry&) const override { return {}; }
};

class TemplateManagerSettingsService final : public luna::editor::SettingsService {
public:
    [[nodiscard]] luna::editor::EditorThemePreset editorTheme() const override
    {
        return luna::editor::EditorThemePreset::ModernLightweight;
    }
    [[nodiscard]] luna::editor::EditorFontSettings editorFont() const override { return {}; }
    [[nodiscard]] std::vector<luna::editor::EditorFontInfo> listEditorFonts() const override { return {}; }
    [[nodiscard]] std::filesystem::path settingsPath() const override { return {}; }
    [[nodiscard]] std::string lastError() const override { return {}; }
    [[nodiscard]] bool restartRequired() const noexcept override { return false; }
    bool setEditorTheme(luna::editor::EditorThemePreset) override { return true; }
    bool setEditorFont(const std::filesystem::path&, float) override { return true; }
    bool resetEditorFont() override { return true; }
    bool save() override { return true; }
};

class TemplateManagerHost final : public luna::editor::EditorPluginManagerHost {
public:
    TemplateManagerHost()
        : command_service(*this)
    {}

    luna::editor::Ui& ui() override { return ui_service; }
    luna::editor::AssetService& assets() override { return asset_service; }
    luna::editor::WindowService& windows() override { return window_service; }
    luna::editor::CommandService& commands() override { return command_service; }
    luna::editor::HistoryService& history() override { return history_service; }
    luna::editor::MaterialService& materials() override { return material_service; }
    luna::editor::MenuService& menus() override { return menu_service; }
    luna::editor::PluginAssetService& pluginAssets() override { return plugin_asset_service; }
    luna::editor::PluginService& plugins() override { return plugin_service; }
    luna::editor::ProjectService& project() override { return project_service; }
    luna::editor::ScriptPluginService& scriptPlugins() override { return script_plugin_service; }
    luna::editor::ScriptService& scripts() override { return script_service; }
    luna::editor::RenderingService& rendering() override { return rendering_service; }
    luna::editor::SceneService& scene() override { return scene_service; }
    luna::editor::SelectionService& selection() override { return selection_service; }
    luna::editor::SettingsService& settings() override { return settings_service; }
    luna::editor::ShortcutService& shortcuts() override { return shortcut_service; }
    luna::editor::RuntimeViewportService& runtimeViewport() override { return runtime_viewport_service; }
    luna::editor::ViewportService& viewport() override { return viewport_service; }

    bool loadPlugin(std::unique_ptr<luna::editor::Plugin>, const std::filesystem::path& = {}) override { return false; }
    void unloadPlugins() override {}
    void registerPluginAssetRoot(std::string_view plugin_id, const std::filesystem::path& root_path) override
    {
        plugin_asset_service.registerPlugin(plugin_id, root_path);
    }
    void cleanupPluginContributions(std::string_view owner_id) override
    {
        viewport_service.destroyViewportsForOwner(owner_id);
        menu_service.removeOwner(owner_id);
        command_service.removeOwner(owner_id);
        window_service.removeOwner(owner_id);
        plugin_asset_service.unregisterPlugin(owner_id);
    }
    luna::editor::ViewportId createSceneViewportForPlugin(std::string_view owner_id, std::string_view debug_name) override
    {
        return viewport_service.createSceneViewportForOwner(debug_name, owner_id);
    }
    bool drawWindow(std::string_view id)
    {
        return window_service.drawWindow(id, *this, ui_service);
    }

    TemplateManagerUi ui_service;
    TemplateManagerAssetService asset_service;
    TemplateManagerMaterialService material_service;
    TemplateManagerWindowService window_service;
    TemplateManagerCommandService command_service;
    TemplateManagerHistoryService history_service;
    TemplateManagerMenuService menu_service;
    TemplateManagerPluginAssetService plugin_asset_service;
    TemplateManagerPluginService plugin_service;
    TemplateManagerProjectService project_service;
    TemplateManagerScriptPluginService script_plugin_service;
    TemplateManagerScriptService script_service;
    TemplateManagerRenderingService rendering_service;
    TemplateManagerSceneService scene_service;
    TemplateManagerSelectionService selection_service;
    TemplateManagerSettingsService settings_service;
    TemplateManagerShortcutService shortcut_service;
    TemplateManagerRuntimeViewportService runtime_viewport_service;
    TemplateManagerViewportService viewport_service;
};

std::filesystem::path absolutePathFromArg(const char* value)
{
    return std::filesystem::absolute(std::filesystem::path(value)).lexically_normal();
}

const luna::editor::EditorPluginPackage*
findPackageById(const std::vector<luna::editor::EditorPluginPackage>& packages, std::string_view package_id)
{
    const auto it = std::find_if(packages.begin(), packages.end(), [package_id](const auto& package) {
        return package.id == package_id;
    });
    return it != packages.end() ? &(*it) : nullptr;
}

const luna::editor::PluginInfo*
findPluginInfoById(const std::vector<luna::editor::PluginInfo>& plugins, std::string_view plugin_id)
{
    const auto it = std::find_if(plugins.begin(), plugins.end(), [plugin_id](const auto& plugin) {
        return plugin.id == plugin_id;
    });
    return it != plugins.end() ? &(*it) : nullptr;
}

void testTemplateEditorPluginManagerLoad(TestContext& context,
                                         const std::filesystem::path& package_root,
                                         const std::filesystem::path& plugin_binary)
{
    context.expect(std::filesystem::exists(package_root / "editor-plugin.yaml"),
                   "SDK template package manifest should exist");

    const luna::editor::EditorPluginManifestLoader loader;
    const std::vector<luna::editor::EditorPluginPackage> packages = loader.loadPackagesFromRoot(package_root);
    const luna::editor::EditorPluginPackage* package = findPackageById(packages, kExpectedPluginId);
    if (!context.expect(package != nullptr, "SDK template manifest should be discoverable from package root")) {
        return;
    }

    context.expect(package->runtime == luna::editor::EditorPluginRuntime::Native,
                   "SDK template manifest runtime should be Native");
    context.expect(package->category == luna::editor::EditorPluginCategory::Tool,
                   "SDK template manifest category should default to Tool");
    context.expect(package->entry_exists, "SDK template manifest entry should exist");
    context.expect(package->resolved_entry_path == plugin_binary,
                   "SDK template manifest should resolve to built template binary");

    TemplateManagerHost host;
    luna::editor::EditorPluginManager manager(host);
    manager.registerPackage(*package);
    context.expect(manager.loadRegisteredPackages(),
                   "EditorPluginManager should load SDK template through real native path");

    const std::vector<luna::editor::PluginInfo> plugin_infos = manager.pluginInfos();
    const luna::editor::PluginInfo* plugin_info = findPluginInfoById(plugin_infos, kExpectedPluginId);
    if (context.expect(plugin_info != nullptr, "SDK template PluginInfo should exist after manager load")) {
        context.expect(plugin_info->runtime == luna::editor::PluginRuntimeKind::Native,
                       "SDK template PluginInfo runtime should be Native");
        context.expect(plugin_info->category == luna::editor::PluginCategoryKind::Tool,
                       "SDK template PluginInfo category should be Tool");
        context.expect(plugin_info->state == luna::editor::PluginLoadState::Loaded,
                       "SDK template PluginInfo state should be Loaded");
        context.expect(plugin_info->status == "Loaded", "SDK template PluginInfo status should be Loaded");
    }

    context.expect(host.command_service.commands.contains(kExpectedCommandId),
                   "manager-loaded SDK template should register its command");
    context.expect(host.window_service.windows.contains(kExpectedWindowId),
                   "manager-loaded SDK template should register its window");
    context.expect(!host.menu_service.menus.empty(),
                   "manager-loaded SDK template should register its menu item");
    context.expect(host.window_service.isWindowOpen(kExpectedWindowId),
                   "manager-loaded SDK template window should start open");

    context.expect(host.drawWindow(kExpectedWindowId),
                   "manager-loaded SDK template window should draw");
    context.expect(host.ui_service.text_count > 0, "manager-loaded SDK template should draw text");
    context.expect(host.plugin_asset_service.read_text_count > 0,
                   "manager-loaded SDK template should read plugin assets");
    context.expect(host.project_service.info_count > 0,
                   "manager-loaded SDK template should read project info");
    context.expect(host.asset_service.list_count > 0,
                   "manager-loaded SDK template should enumerate project assets");
    context.expect(host.ui_service.section_count > 0 && host.ui_service.combo_count > 0 &&
                       host.ui_service.tree_count > 0,
                   "manager-loaded SDK template should draw advanced UI wrappers");
    context.expect(host.ui_service.drag_drop_source_count > 0 && host.ui_service.drag_drop_target_count > 0,
                   "manager-loaded SDK template should draw drag/drop wrappers");
    context.expect(host.ui_service.tooltip_count > 0, "manager-loaded SDK template should use item tooltip wrapper");
    context.expect(host.ui_service.image_count > 0, "manager-loaded SDK template should draw viewport image");
    context.expect(host.ui_service.heading_count > 0, "manager-loaded SDK template should draw styled heading UI");
    context.expect(host.ui_service.key_value_count > 0, "manager-loaded SDK template should draw styled key/value UI");
    context.expect(host.ui_service.badge_count > 0, "manager-loaded SDK template should draw styled badge UI");
    context.expect(host.ui_service.metric_count > 0, "manager-loaded SDK template should draw styled metric UI");
    context.expect(host.ui_service.asset_field_count > 0, "manager-loaded SDK template should draw styled asset field UI");
    context.expect(host.ui_service.asset_drop_count > 0,
                   "manager-loaded SDK template should accept asset drops through semantic UI ABI");
    context.expect(host.ui_service.panel_depth == 0, "manager-loaded SDK template should balance styled panel scopes");
    context.expect(!host.viewport_service.viewports.empty(),
                   "manager-loaded SDK template should create an independent scene viewport");

    host.ui_service.next_button_pressed = true;
    context.expect(host.drawWindow(kExpectedWindowId),
                   "manager-loaded SDK template button draw should run");
    context.expect(host.scene_service.created_entity_count == 1,
                   "manager-loaded SDK template button should create an entity");
    context.expect(static_cast<uint64_t>(host.selection_service.selectedEntityId()) >= 100u,
                   "manager-loaded SDK template should select its created entity");

    manager.unloadAll();
    context.expect(host.command_service.commands.empty(), "manager unload should remove SDK template commands");
    context.expect(host.window_service.windows.empty(), "manager unload should remove SDK template windows");
    context.expect(host.menu_service.menus.empty(), "manager unload should remove SDK template menu items");
    context.expect(host.viewport_service.viewports.empty(), "manager unload should remove SDK template viewports");
    context.expect(host.plugin_asset_service.roots.empty(), "manager unload should unregister SDK template asset root");
}

} // namespace

int main(int argc, char** argv)
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    if (argc != 3) {
        context.expect(false, "expected plugin binary path and package root arguments");
        luna::Logger::shutdown();
        return context.result();
    }

    const std::filesystem::path plugin_binary = absolutePathFromArg(argv[1]);
    const std::filesystem::path package_root = absolutePathFromArg(argv[2]);
    context.expect(std::filesystem::exists(plugin_binary), "SDK template plugin binary should exist");
    context.expect(std::filesystem::exists(package_root), "SDK template package root should exist");

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
    context.expect(host.section_count > 0, "SDK template should draw through section UI wrapper");
    context.expect(host.combo_count > 0, "SDK template should draw through combo UI wrapper");
    context.expect(host.tree_count > 0, "SDK template should draw through tree UI wrapper");
    context.expect(host.drag_drop_source_count > 0, "SDK template should draw through drag/drop source wrapper");
    context.expect(host.drag_drop_target_count > 0, "SDK template should draw through drag/drop target wrapper");
    context.expect(host.tooltip_count > 0, "SDK template should draw through item hover/tooltip wrappers");
    context.expect(host.image_count > 0, "SDK template should draw a scene viewport texture");
    context.expect(host.heading_count > 0, "SDK template should draw through styled heading UI wrapper");
    context.expect(host.key_value_count > 0, "SDK template should draw through styled key/value UI wrapper");
    context.expect(host.badge_count > 0, "SDK template should draw through styled badge UI wrapper");
    context.expect(host.metric_count > 0, "SDK template should draw through styled metric UI wrapper");
    context.expect(host.asset_field_count > 0, "SDK template should draw through styled asset field UI wrapper");
    context.expect(host.asset_drop_count > 0, "SDK template should accept asset drops through semantic UI wrapper");
    context.expect(host.panel_depth == 0, "SDK template should balance styled panel scopes");
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

    testTemplateEditorPluginManagerLoad(context, package_root, plugin_binary);

    luna::Logger::shutdown();
    return context.result();
}
