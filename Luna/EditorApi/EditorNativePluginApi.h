#pragma once

#include <stddef.h>
#include <stdint.h>

#define LUNA_EDITOR_HOST_API_VERSION 1u
#define LUNA_EDITOR_PLUGIN_API_VERSION 1u
#define LUNA_EDITOR_LOG_API_VERSION 1u
#define LUNA_EDITOR_UI_API_VERSION 1u
#define LUNA_EDITOR_COMMAND_API_VERSION 1u
#define LUNA_EDITOR_WINDOW_API_VERSION 1u
#define LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION 1u
#define LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION 1u
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

typedef struct LunaEditorHostApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* host_user_data;
    LunaEditorLogApi log;
    LunaEditorUiApi ui;
    LunaEditorCommandApi commands;
    LunaEditorWindowApi windows;
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
