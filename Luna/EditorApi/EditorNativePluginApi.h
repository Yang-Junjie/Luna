#pragma once

#include <stddef.h>
#include <stdint.h>

#define LUNA_EDITOR_HOST_API_VERSION 1u
#define LUNA_EDITOR_PLUGIN_API_VERSION 1u
#define LUNA_EDITOR_LOG_API_VERSION 1u
#define LUNA_EDITOR_UI_API_VERSION 1u
#define LUNA_EDITOR_COMMAND_API_VERSION 1u
#define LUNA_EDITOR_WINDOW_API_VERSION 1u
#define LUNA_EDITOR_ASSET_API_VERSION 1u
#define LUNA_EDITOR_PLUGIN_ASSET_API_VERSION 1u
#define LUNA_EDITOR_MENU_API_VERSION 1u
#define LUNA_EDITOR_PROJECT_API_VERSION 1u
#define LUNA_EDITOR_SCENE_API_VERSION 1u
#define LUNA_EDITOR_SELECTION_API_VERSION 1u
#define LUNA_EDITOR_VIEWPORT_API_VERSION 1u
#define LUNA_EDITOR_RUNTIME_VIEWPORT_API_VERSION 1u
#define LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION 1u
#define LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION 1u
#define LUNA_EDITOR_SCENE_MESH_COMPONENT_API_VERSION 1u
#define LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION 1u
#define LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION 1u
#define LUNA_EDITOR_ASSET_INFO_API_VERSION 1u
#define LUNA_EDITOR_ASSET_REFRESH_RESULT_API_VERSION 1u
#define LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION 1u
#define LUNA_EDITOR_PROJECT_INFO_API_VERSION 1u
#define LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION 1u
#define LUNA_EDITOR_SCENE_ENTITY_CREATE_REQUEST_API_VERSION 1u
#define LUNA_EDITOR_CREATE_PLUGIN_SYMBOL "LunaCreateEditorPlugin"

#if defined(_WIN32)
#    define LUNA_EDITOR_PLUGIN_EXPORT __declspec(dllexport)
#else
#    define LUNA_EDITOR_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LunaEditorVec2 {
    float x;
    float y;
} LunaEditorVec2;

typedef struct LunaEditorVec3 {
    float x;
    float y;
    float z;
} LunaEditorVec3;

typedef struct LunaEditorVec4 {
    float x;
    float y;
    float z;
    float w;
} LunaEditorVec4;

typedef struct LunaEditorTextureView {
    uintptr_t texture_id;
    uint32_t width;
    uint32_t height;
    int32_t y_flip;
} LunaEditorTextureView;

typedef enum LunaEditorLogLevel {
    LunaEditorLogLevel_Trace = 0,
    LunaEditorLogLevel_Info = 1,
    LunaEditorLogLevel_Warn = 2,
    LunaEditorLogLevel_Error = 3,
} LunaEditorLogLevel;

typedef enum LunaEditorButtonVariant {
    LunaEditorButtonVariant_Default = 0,
    LunaEditorButtonVariant_Primary = 1,
    LunaEditorButtonVariant_Danger = 2,
    LunaEditorButtonVariant_Subtle = 3,
} LunaEditorButtonVariant;

typedef enum LunaEditorWindowFlag {
    LunaEditorWindowFlag_None = 0,
    LunaEditorWindowFlag_NoSavedSettings = 1u << 0,
    LunaEditorWindowFlag_NoDocking = 1u << 1,
    LunaEditorWindowFlag_NoPadding = 1u << 2,
} LunaEditorWindowFlag;

typedef enum LunaEditorTableFlag {
    LunaEditorTableFlag_None = 0,
    LunaEditorTableFlag_RowBg = 1u << 0,
    LunaEditorTableFlag_BordersInnerH = 1u << 1,
    LunaEditorTableFlag_BordersInnerV = 1u << 2,
    LunaEditorTableFlag_SizingStretchProp = 1u << 3,
    LunaEditorTableFlag_ScrollY = 1u << 4,
} LunaEditorTableFlag;

typedef enum LunaEditorTableColumnFlag {
    LunaEditorTableColumnFlag_None = 0,
    LunaEditorTableColumnFlag_WidthFixed = 1u << 0,
    LunaEditorTableColumnFlag_WidthStretch = 1u << 1,
} LunaEditorTableColumnFlag;

typedef enum LunaEditorTreeNodeFlag {
    LunaEditorTreeNodeFlag_None = 0,
    LunaEditorTreeNodeFlag_OpenOnArrow = 1u << 0,
    LunaEditorTreeNodeFlag_OpenOnDoubleClick = 1u << 1,
    LunaEditorTreeNodeFlag_SpanAvailWidth = 1u << 2,
    LunaEditorTreeNodeFlag_Leaf = 1u << 3,
    LunaEditorTreeNodeFlag_NoTreePushOnOpen = 1u << 4,
    LunaEditorTreeNodeFlag_Selected = 1u << 5,
    LunaEditorTreeNodeFlag_DefaultOpen = 1u << 6,
    LunaEditorTreeNodeFlag_FramePadding = 1u << 7,
} LunaEditorTreeNodeFlag;

typedef enum LunaEditorMouseButton {
    LunaEditorMouseButton_Left = 0,
    LunaEditorMouseButton_Right = 1,
    LunaEditorMouseButton_Middle = 2,
} LunaEditorMouseButton;

typedef enum LunaEditorAssetType {
    LunaEditorAssetType_None = 0,
    LunaEditorAssetType_Texture = 1,
    LunaEditorAssetType_Mesh = 2,
    LunaEditorAssetType_Material = 3,
    LunaEditorAssetType_Model = 4,
    LunaEditorAssetType_Scene = 5,
    LunaEditorAssetType_Script = 6,
} LunaEditorAssetType;

typedef enum LunaEditorSceneEntityCreateKind {
    LunaEditorSceneEntityCreateKind_Empty = 0,
    LunaEditorSceneEntityCreateKind_Camera = 1,
    LunaEditorSceneEntityCreateKind_DirectionalLight = 2,
    LunaEditorSceneEntityCreateKind_PointLight = 3,
    LunaEditorSceneEntityCreateKind_SpotLight = 4,
    LunaEditorSceneEntityCreateKind_PrimitiveMesh = 5,
    LunaEditorSceneEntityCreateKind_MeshAsset = 6,
    LunaEditorSceneEntityCreateKind_ModelAsset = 7,
} LunaEditorSceneEntityCreateKind;

typedef enum LunaEditorSceneComponentKind {
    LunaEditorSceneComponentKind_Transform = 0,
    LunaEditorSceneComponentKind_Camera = 1,
    LunaEditorSceneComponentKind_Light = 2,
    LunaEditorSceneComponentKind_Mesh = 3,
    LunaEditorSceneComponentKind_Script = 4,
} LunaEditorSceneComponentKind;

typedef enum LunaEditorSceneEntityComponentFlag {
    LunaEditorSceneEntityComponentFlag_None = 0,
    LunaEditorSceneEntityComponentFlag_Transform = 1u << 0,
    LunaEditorSceneEntityComponentFlag_Camera = 1u << 1,
    LunaEditorSceneEntityComponentFlag_Light = 1u << 2,
    LunaEditorSceneEntityComponentFlag_Mesh = 1u << 3,
    LunaEditorSceneEntityComponentFlag_Script = 1u << 4,
} LunaEditorSceneEntityComponentFlag;

struct LunaEditorHostApi;

typedef struct LunaEditorLogApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    void (*log)(void* api_user_data, LunaEditorLogLevel level, const char* message);
} LunaEditorLogApi;

typedef struct LunaEditorUiApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;

    int (*begin_window)(void* api_user_data,
                        const char* id,
                        const char* title,
                        int* open,
                        uint32_t flags);
    void (*end_window)(void* api_user_data);

    void (*text)(void* api_user_data, const char* value);
    void (*text_disabled)(void* api_user_data, const char* value);
    void (*text_wrapped)(void* api_user_data, const char* value);
    void (*bullet_text)(void* api_user_data, const char* value);
    void (*separator)(void* api_user_data);
    void (*separator_text)(void* api_user_data, const char* label);
    void (*same_line)(void* api_user_data);
    void (*spacing)(void* api_user_data);
    void (*indent)(void* api_user_data, float width);
    void (*unindent)(void* api_user_data, float width);
    void (*begin_disabled)(void* api_user_data);
    void (*end_disabled)(void* api_user_data);
    void (*set_next_item_width)(void* api_user_data, float width);
    void (*content_region_avail)(void* api_user_data, LunaEditorVec2* out_value);
    void (*window_framebuffer_scale)(void* api_user_data, LunaEditorVec2* out_value);

    int (*button)(void* api_user_data,
                  const char* label,
                  const LunaEditorVec2* size,
                  uint32_t variant);
    int (*checkbox)(void* api_user_data, const char* label, int* value);
    int (*color_edit3)(void* api_user_data, const char* label, LunaEditorVec3* value);
    int (*color_edit4)(void* api_user_data, const char* label, LunaEditorVec4* value);
    int (*slider_int)(void* api_user_data, const char* label, int* value, int min_value, int max_value);
    int (*slider_float)(void* api_user_data,
                        const char* label,
                        float* value,
                        float min_value,
                        float max_value,
                        const char* format);
    int (*drag_int)(void* api_user_data,
                    const char* label,
                    int* value,
                    float speed,
                    int min_value,
                    int max_value);
    int (*drag_float)(void* api_user_data,
                      const char* label,
                      float* value,
                      float speed,
                      float min_value,
                      float max_value,
                      const char* format);
    int (*drag_float3)(void* api_user_data,
                       const char* label,
                       LunaEditorVec3* value,
                       float speed,
                       float min_value,
                       float max_value,
                       const char* format);
    int (*input_text)(void* api_user_data, const char* label, char* value, size_t buffer_size);
    int (*input_text_with_hint)(void* api_user_data,
                                const char* label,
                                const char* hint,
                                char* value,
                                size_t buffer_size);

    int (*tree_node)(void* api_user_data, const char* label);
    int (*tree_node_ex)(void* api_user_data, const char* id, const char* label, uint32_t flags);
    void (*tree_pop)(void* api_user_data);
    int (*begin_combo)(void* api_user_data, const char* label, const char* preview_value);
    void (*end_combo)(void* api_user_data);
    int (*selectable)(void* api_user_data, const char* label, int selected);
    void (*set_item_default_focus)(void* api_user_data);
    int (*image)(void* api_user_data, const LunaEditorTextureView* texture, const LunaEditorVec2* size);
    int (*is_item_hovered)(void* api_user_data);
    int (*is_item_clicked)(void* api_user_data, int button);
    int (*is_item_double_clicked)(void* api_user_data, int button);
    int (*is_item_deactivated_after_edit)(void* api_user_data);
    void (*set_tooltip)(void* api_user_data, const char* value);
    int (*invisible_button)(void* api_user_data, const char* id, const LunaEditorVec2* size);

    int (*begin_section)(void* api_user_data, const char* id, const char* label, int default_open);
    void (*end_section)(void* api_user_data);
    int (*begin_menu)(void* api_user_data, const char* label, int enabled);
    void (*end_menu)(void* api_user_data);
    int (*menu_item)(void* api_user_data, const char* label, int selected, int enabled);
    void (*open_popup)(void* api_user_data, const char* id);
    int (*begin_popup)(void* api_user_data, const char* id);
    int (*begin_popup_context_item)(void* api_user_data, const char* id, int button);
    void (*close_current_popup)(void* api_user_data);
    void (*end_popup)(void* api_user_data);

    int (*begin_drag_drop_source)(void* api_user_data);
    int (*set_drag_drop_payload)(void* api_user_data, const char* type, const void* data, size_t size);
    void (*end_drag_drop_source)(void* api_user_data);
    int (*begin_drag_drop_target)(void* api_user_data);
    int (*accept_drag_drop_payload)(void* api_user_data, const char* type, void* out_data, size_t size);
    void (*end_drag_drop_target)(void* api_user_data);

    float (*scale)(void* api_user_data, float value);
    void (*scaled)(void* api_user_data, const LunaEditorVec2* value, LunaEditorVec2* out_value);

    int (*begin_table)(void* api_user_data,
                       const char* id,
                       int column_count,
                       uint32_t flags,
                       const LunaEditorVec2* outer_size);
    void (*end_table)(void* api_user_data);
    void (*table_setup_column)(void* api_user_data,
                               const char* label,
                               uint32_t flags,
                               float init_width_or_weight);
    void (*table_headers_row)(void* api_user_data);
    void (*table_next_row)(void* api_user_data);
    int (*table_next_column)(void* api_user_data);
} LunaEditorUiApi;

typedef struct LunaEditorCommandDescriptor {
    uint32_t struct_size;
    uint32_t api_version;
    const char* id;
    const char* label;
    const char* description;
    const char* shortcut;
    void* command_user_data;
    int (*can_execute)(void* command_user_data, const struct LunaEditorHostApi* host_api);
    int (*is_checked)(void* command_user_data, const struct LunaEditorHostApi* host_api);
    void (*execute)(void* command_user_data, const struct LunaEditorHostApi* host_api);
} LunaEditorCommandDescriptor;

typedef struct LunaEditorWindowDescriptor {
    uint32_t struct_size;
    uint32_t api_version;
    const char* id;
    const char* title;
    int default_open;
    LunaEditorVec2 default_size;
    uint32_t flags;
    void* window_user_data;
    void (*draw)(void* window_user_data, const struct LunaEditorHostApi* host_api);
} LunaEditorWindowDescriptor;

typedef struct LunaEditorCommandApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*register_command)(void* api_user_data, const LunaEditorCommandDescriptor* descriptor);
    void (*unregister_command)(void* api_user_data, const char* id);
    int (*execute_command)(void* api_user_data, const char* id);
    int (*can_execute_command)(void* api_user_data, const char* id);
    int (*is_command_checked)(void* api_user_data, const char* id);
} LunaEditorCommandApi;

typedef struct LunaEditorWindowApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*register_window)(void* api_user_data, const LunaEditorWindowDescriptor* descriptor);
    void (*unregister_window)(void* api_user_data, const char* id);
    int (*is_window_open)(void* api_user_data, const char* id);
    void (*set_window_open)(void* api_user_data, const char* id, int open);
} LunaEditorWindowApi;

typedef struct LunaEditorAssetInfo {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t handle;
    uint32_t type;
    int exists;
    int builtin;
    int loading;
    int memory_only;
    char* label;
    size_t label_size;
    char* detail;
    size_t detail_size;
    char* project_path;
    size_t project_path_size;
    char* absolute_path;
    size_t absolute_path_size;
} LunaEditorAssetInfo;

typedef struct LunaEditorAssetRefreshResult {
    uint32_t struct_size;
    uint32_t api_version;
    int success;
    int project_loaded;
    uint64_t revision;
    char* message;
    size_t message_size;
    size_t discovered_assets;
    size_t imported_missing_assets;
    size_t loaded_existing_metadata;
    size_t rebuilt_metadata;
    size_t unsupported_files_skipped;
    size_t failed_assets;
    size_t missing_metadata_after_sync;
    size_t script_files_skipped_no_plugin;
    size_t script_files_skipped_unsupported_language;
    size_t generated_model_files;
    size_t generated_model_metadata;
    size_t generated_material_files;
    size_t generated_material_metadata;
    size_t generated_texture_metadata;
    size_t failed_generated_model_assets;
} LunaEditorAssetRefreshResult;

typedef int (*LunaEditorEnumerateAssetFn)(void* user_data, const LunaEditorAssetInfo* asset_info);

typedef struct LunaEditorAssetApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*describe_asset)(void* api_user_data, uint64_t handle, LunaEditorAssetInfo* out_info);
    int (*asset_info)(void* api_user_data, uint64_t handle, LunaEditorAssetInfo* out_info);
    int (*asset_info_by_path)(void* api_user_data, const char* path, LunaEditorAssetInfo* out_info);
    size_t (*list_assets)(void* api_user_data,
                          uint32_t type_filter,
                          int include_builtin,
                          void* user_data,
                          LunaEditorEnumerateAssetFn enumerate_fn);
    int (*asset_exists)(void* api_user_data, uint64_t handle);
    int (*asset_path_exists)(void* api_user_data, const char* path);
    uint64_t (*find_asset_handle_by_path)(void* api_user_data, const char* path);
    int (*assets_root_path)(void* api_user_data, char* out_path, size_t out_path_size);
    int (*resolve_project_asset_path)(void* api_user_data,
                                      const char* project_relative_path,
                                      char* out_path,
                                      size_t out_path_size);
    int (*make_project_relative_asset_path)(void* api_user_data,
                                            const char* path,
                                            char* out_path,
                                            size_t out_path_size);
    int (*refresh_assets)(void* api_user_data, LunaEditorAssetRefreshResult* out_result);
    uint64_t (*asset_revision)(void* api_user_data);
    int (*is_asset_loading)(void* api_user_data, uint64_t handle);
    int (*accepts_asset_type)(void* api_user_data,
                              uint32_t type,
                              const uint32_t* accepted_types,
                              size_t accepted_type_count);
    int (*mesh_submesh_count)(void* api_user_data, uint64_t mesh_handle, size_t* out_count);
    int (*begin_asset_drag_drop_source)(void* api_user_data, uint64_t handle, const char* label);
} LunaEditorAssetApi;

typedef struct LunaEditorPluginAssetApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*plugin_root_path)(void* api_user_data, char* out_path, size_t out_path_size);
    int (*asset_root_path)(void* api_user_data, char* out_path, size_t out_path_size);
    int (*resolve_path)(void* api_user_data, const char* relative_asset_path, char* out_path, size_t out_path_size);
    int (*exists)(void* api_user_data, const char* relative_asset_path);
    int (*read_text)(void* api_user_data,
                     const char* relative_asset_path,
                     char* out_text,
                     size_t out_text_size,
                     size_t* out_required_size);
    int (*read_bytes)(void* api_user_data,
                      const char* relative_asset_path,
                      void* out_data,
                      size_t out_data_size,
                      size_t* out_required_size);
    int (*texture)(void* api_user_data, const char* relative_asset_path, LunaEditorTextureView* out_texture);
} LunaEditorPluginAssetApi;

typedef struct LunaEditorMenuItemDescriptor {
    uint32_t struct_size;
    uint32_t api_version;
    const char* menu_path;
    const char* command_id;
    const char* label;
    const char* shortcut;
} LunaEditorMenuItemDescriptor;

typedef struct LunaEditorMenuApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*add_menu_item)(void* api_user_data, const LunaEditorMenuItemDescriptor* descriptor);
    void (*remove_menu_item)(void* api_user_data, const char* menu_path, const char* command_id);
    void (*remove_menu_items_for_command)(void* api_user_data, const char* command_id);
} LunaEditorMenuApi;

typedef struct LunaEditorProjectInfo {
    uint32_t struct_size;
    uint32_t api_version;
    char* name;
    size_t name_size;
    char* version;
    size_t version_size;
    char* author;
    size_t author_size;
    char* description;
    size_t description_size;
    char* start_scene;
    size_t start_scene_size;
    char* assets_path;
    size_t assets_path_size;
    char* selected_script_plugin_id;
    size_t selected_script_plugin_id_size;
    char* selected_script_backend_name;
    size_t selected_script_backend_name_size;
} LunaEditorProjectInfo;

typedef struct LunaEditorProjectApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*has_project_loaded)(void* api_user_data);
    int (*project_root_path)(void* api_user_data, char* out_path, size_t out_path_size);
    int (*project_info)(void* api_user_data, LunaEditorProjectInfo* out_info);
    int (*save_project)(void* api_user_data);
} LunaEditorProjectApi;

typedef struct LunaEditorSceneTransform {
    LunaEditorVec3 translation;
    LunaEditorVec3 rotation_degrees;
    LunaEditorVec3 scale;
} LunaEditorSceneTransform;

typedef struct LunaEditorSceneCameraComponent {
    uint32_t struct_size;
    uint32_t api_version;
    int primary;
    int fixed_aspect_ratio;
    uint32_t projection;
    float perspective_vertical_fov_degrees;
    float perspective_near;
    float perspective_far;
    float orthographic_size;
    float orthographic_near;
    float orthographic_far;
} LunaEditorSceneCameraComponent;

typedef struct LunaEditorSceneLightComponent {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t type;
    int enabled;
    LunaEditorVec3 color;
    float intensity;
    float range;
    float inner_cone_angle_degrees;
    float outer_cone_angle_degrees;
} LunaEditorSceneLightComponent;

typedef struct LunaEditorSceneMeshComponent {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t mesh_handle;
    uint32_t first_submesh;
    uint32_t submesh_count;
    uint64_t* submesh_material_handles;
    size_t submesh_material_capacity;
    size_t submesh_material_count;
} LunaEditorSceneMeshComponent;

typedef struct LunaEditorViewportPresentation {
    uint32_t struct_size;
    uint32_t api_version;
    LunaEditorTextureView scene_texture;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    int presentable;
} LunaEditorViewportPresentation;

typedef struct LunaEditorSceneEntityInfo {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t id;
    uint64_t parent_id;
    uint32_t component_flags;
    size_t child_count;
    char* name;
    size_t name_size;
    char* parent_name;
    size_t parent_name_size;
} LunaEditorSceneEntityInfo;

typedef struct LunaEditorSceneEntityCreateRequest {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t kind;
    const char* name;
    uint64_t parent_id;
    uint64_t asset_handle;
} LunaEditorSceneEntityCreateRequest;

typedef int (*LunaEditorEnumerateSceneEntityFn)(void* user_data, const LunaEditorSceneEntityInfo* entity_info);

typedef struct LunaEditorSceneApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*scene_label)(void* api_user_data, char* out_label, size_t out_label_size);
    size_t (*entity_count)(void* api_user_data);
    int (*can_edit_scene)(void* api_user_data);
    int (*open_scene_file)(void* api_user_data, const char* scene_file_path);
    size_t (*enumerate_entities)(void* api_user_data,
                                 void* user_data,
                                 LunaEditorEnumerateSceneEntityFn enumerate_fn);
    int (*entity_exists)(void* api_user_data, uint64_t entity_id);
    int (*entity_info)(void* api_user_data, uint64_t entity_id, LunaEditorSceneEntityInfo* out_info);
    int (*is_entity_descendant_of)(void* api_user_data, uint64_t entity_id, uint64_t potential_ancestor_id);
    uint64_t (*create_entity)(void* api_user_data, const char* name);
    uint64_t (*create_entity_ex)(void* api_user_data, const LunaEditorSceneEntityCreateRequest* request);
    int (*destroy_entity)(void* api_user_data, uint64_t entity_id);
    int (*reparent_entity)(void* api_user_data, uint64_t entity_id, uint64_t new_parent_id, int preserve_world_transform);
    int (*set_entity_name)(void* api_user_data, uint64_t entity_id, const char* name);
    int (*get_entity_transform)(void* api_user_data, uint64_t entity_id, LunaEditorSceneTransform* out_transform);
    int (*set_entity_transform)(void* api_user_data, uint64_t entity_id, const LunaEditorSceneTransform* transform);
    int (*get_camera_component)(void* api_user_data, uint64_t entity_id, LunaEditorSceneCameraComponent* out_component);
    int (*set_camera_component)(void* api_user_data, uint64_t entity_id, const LunaEditorSceneCameraComponent* component);
    int (*get_light_component)(void* api_user_data, uint64_t entity_id, LunaEditorSceneLightComponent* out_component);
    int (*set_light_component)(void* api_user_data, uint64_t entity_id, const LunaEditorSceneLightComponent* component);
    int (*get_mesh_component)(void* api_user_data, uint64_t entity_id, LunaEditorSceneMeshComponent* out_component);
    int (*set_mesh_component)(void* api_user_data, uint64_t entity_id, const LunaEditorSceneMeshComponent* component);
    int (*add_component)(void* api_user_data, uint64_t entity_id, uint32_t component_kind);
    int (*remove_component)(void* api_user_data, uint64_t entity_id, uint32_t component_kind);
} LunaEditorSceneApi;

typedef struct LunaEditorSelectionApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    uint64_t (*selected_entity_id)(void* api_user_data);
    void (*select_entity)(void* api_user_data, uint64_t entity_id);
    void (*clear_selection)(void* api_user_data);
} LunaEditorSelectionApi;

typedef struct LunaEditorViewportApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*sync_scene_viewport)(void* api_user_data,
                               uint32_t framebuffer_width,
                               uint32_t framebuffer_height,
                               LunaEditorViewportPresentation* out_presentation);
    int (*scene_texture_view)(void* api_user_data, LunaEditorTextureView* out_texture);
    void (*editor_camera_position)(void* api_user_data, LunaEditorVec3* out_position);
    int (*gizmo_operation_name)(void* api_user_data, char* out_value, size_t out_value_size);
    int (*gizmo_mode_name)(void* api_user_data, char* out_value, size_t out_value_size);
    int (*pick_debug_visualization_enabled)(void* api_user_data);
    void (*set_pick_debug_visualization_enabled)(void* api_user_data, int enabled);
    int (*editor_grid_enabled)(void* api_user_data);
    void (*set_editor_grid_enabled)(void* api_user_data, int enabled);
} LunaEditorViewportApi;

typedef struct LunaEditorRuntimeViewportApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* api_user_data;
    int (*is_runtime_viewport_enabled)(void* api_user_data);
    int (*is_runtime_viewport_requested)(void* api_user_data);
    void (*set_runtime_viewport_requested)(void* api_user_data, int enabled);
    size_t (*runtime_entity_count)(void* api_user_data);
} LunaEditorRuntimeViewportApi;

typedef struct LunaEditorHostApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* host_user_data;
    LunaEditorLogApi log;
    LunaEditorUiApi ui;
    LunaEditorCommandApi commands;
    LunaEditorWindowApi windows;
    LunaEditorAssetApi assets;
    LunaEditorPluginAssetApi plugin_assets;
    LunaEditorMenuApi menus;
    LunaEditorProjectApi project;
    LunaEditorSceneApi scene;
    LunaEditorSelectionApi selection;
    LunaEditorViewportApi viewport;
    LunaEditorRuntimeViewportApi runtime_viewport;
} LunaEditorHostApi;

typedef struct LunaEditorPluginApi {
    uint32_t struct_size;
    uint32_t api_version;
    const char* plugin_id;
    const char* display_name;
    const char* version;
    void* plugin_user_data;

    int (*on_load)(void* plugin_user_data, const LunaEditorHostApi* host_api);
    void (*on_unload)(void* plugin_user_data, const LunaEditorHostApi* host_api);
} LunaEditorPluginApi;

typedef int (*LunaCreateEditorPluginFn)(uint32_t host_api_version,
                                        const LunaEditorHostApi* host_api,
                                        LunaEditorPluginApi* out_plugin_api);

#ifdef __cplusplus
}
#endif
