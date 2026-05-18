#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    // Script host ABI v1.
    // Stable modules in this version: Core(Log), Scene, Entity/Transform, Input.
    LUNA_SCRIPT_HOST_API_VERSION = 1u,
};

typedef struct LunaScriptVec3 {
    float x;
    float y;
    float z;
} LunaScriptVec3;

typedef enum LunaScriptPropertyType {
    LunaScriptPropertyType_Bool = 0,
    LunaScriptPropertyType_Int = 1,
    LunaScriptPropertyType_Float = 2,
    LunaScriptPropertyType_String = 3,
    LunaScriptPropertyType_Vec3 = 4,
    LunaScriptPropertyType_Entity = 5,
    LunaScriptPropertyType_Asset = 6,
} LunaScriptPropertyType;

typedef enum LunaScriptCameraProjectionType {
    LunaScriptCameraProjectionType_Perspective = 0,
    LunaScriptCameraProjectionType_Orthographic = 1,
} LunaScriptCameraProjectionType;

typedef enum LunaScriptKeyCode {
    LunaScriptKeyCode_None = 0,
    LunaScriptKeyCode_LeftShift = 1,
    LunaScriptKeyCode_RightShift = 2,
    LunaScriptKeyCode_LeftControl = 3,
    LunaScriptKeyCode_RightControl = 4,
    LunaScriptKeyCode_LeftAlt = 5,
    LunaScriptKeyCode_RightAlt = 6,
    LunaScriptKeyCode_Space = 7,
    LunaScriptKeyCode_Enter = 8,
    LunaScriptKeyCode_Delete = 9,
    LunaScriptKeyCode_Escape = 10,
    LunaScriptKeyCode_Up = 11,
    LunaScriptKeyCode_Down = 12,
    LunaScriptKeyCode_Left = 13,
    LunaScriptKeyCode_Right = 14,
    LunaScriptKeyCode_Backspace = 15,
    LunaScriptKeyCode_A = 65,
    LunaScriptKeyCode_B = 66,
    LunaScriptKeyCode_C = 67,
    LunaScriptKeyCode_D = 68,
    LunaScriptKeyCode_E = 69,
    LunaScriptKeyCode_F = 70,
    LunaScriptKeyCode_G = 71,
    LunaScriptKeyCode_H = 72,
    LunaScriptKeyCode_I = 73,
    LunaScriptKeyCode_J = 74,
    LunaScriptKeyCode_K = 75,
    LunaScriptKeyCode_L = 76,
    LunaScriptKeyCode_M = 77,
    LunaScriptKeyCode_N = 78,
    LunaScriptKeyCode_O = 79,
    LunaScriptKeyCode_P = 80,
    LunaScriptKeyCode_Q = 81,
    LunaScriptKeyCode_R = 82,
    LunaScriptKeyCode_S = 83,
    LunaScriptKeyCode_T = 84,
    LunaScriptKeyCode_U = 85,
    LunaScriptKeyCode_V = 86,
    LunaScriptKeyCode_W = 87,
    LunaScriptKeyCode_X = 88,
    LunaScriptKeyCode_Y = 89,
    LunaScriptKeyCode_Z = 90,
} LunaScriptKeyCode;

typedef enum LunaScriptMouseCode {
    LunaScriptMouseCode_None = 0,
    LunaScriptMouseCode_Left = 1,
    LunaScriptMouseCode_Right = 2,
    LunaScriptMouseCode_Middle = 3,
    LunaScriptMouseCode_XButton1 = 4,
    LunaScriptMouseCode_XButton2 = 5,
} LunaScriptMouseCode;

typedef enum LunaScriptCursorMode {
    LunaScriptCursorMode_Normal = 0,
    LunaScriptCursorMode_Hidden = 1,
    LunaScriptCursorMode_Locked = 2,
} LunaScriptCursorMode;

typedef enum LunaScriptHostLogLevel {
    LunaScriptHostLogLevel_Trace = 0,
    LunaScriptHostLogLevel_Info = 1,
    LunaScriptHostLogLevel_Warn = 2,
    LunaScriptHostLogLevel_Error = 3,
} LunaScriptHostLogLevel;

typedef struct LunaScriptPropertyDesc {
    const char* name;
    LunaScriptPropertyType type;
    int32_t bool_value;
    int32_t int_value;
    float float_value;
    const char* string_value;
    LunaScriptVec3 vec3_value;
    uint64_t entity_value;
    uint64_t asset_value;
} LunaScriptPropertyDesc;

typedef struct LunaScriptCameraDesc {
    int32_t primary;
    int32_t fixed_aspect_ratio;
    LunaScriptCameraProjectionType projection_type;
    float perspective_vertical_fov_radians;
    float perspective_near;
    float perspective_far;
    float orthographic_size;
    float orthographic_near;
    float orthographic_far;
} LunaScriptCameraDesc;

typedef struct LunaScriptInstanceDesc {
    uint64_t entity_id;
    const char* entity_name;
    uint64_t script_id;
    uint64_t script_asset;
    const char* type_name;
    const char* asset_name;
    const char* language;
    const char* source;
    int32_t execution_order;
} LunaScriptInstanceDesc;

typedef void (*LunaScriptHostLogFn)(void* user_data, LunaScriptHostLogLevel level, const char* message);
typedef int (*LunaScriptEnumerateInstancesFn)(void* user_data, const LunaScriptInstanceDesc* script_instance);
typedef int (*LunaScriptEnumeratePropertiesFn)(void* user_data, const LunaScriptPropertyDesc* script_property);

typedef struct LunaScriptHostApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* user_data;

    // Core
    LunaScriptHostLogFn log;

    // Scene
    const char* (*scene_get_name)(void* scene_context);
    int (*scene_enumerate_script_instances)(void* scene_context,
                                            void* user_data,
                                            LunaScriptEnumerateInstancesFn enumerate_fn);
    int (*scene_enumerate_script_properties)(void* scene_context,
                                             uint64_t entity_id,
                                             uint64_t script_id,
                                             void* user_data,
                                             LunaScriptEnumeratePropertiesFn enumerate_fn);

    // Entity / Transform
    int (*entity_is_valid)(void* scene_context, uint64_t entity_id);
    const char* (*entity_get_name)(void* scene_context, uint64_t entity_id);
    int (*entity_get_translation)(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value);
    int (*entity_set_translation)(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value);
    int (*entity_get_rotation)(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value);
    int (*entity_set_rotation)(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value);
    int (*entity_get_scale)(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value);
    int (*entity_set_scale)(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value);
    int (*entity_has_camera)(void* scene_context, uint64_t entity_id);
    int (*entity_get_camera)(void* scene_context, uint64_t entity_id, LunaScriptCameraDesc* out_camera);
    int (*entity_set_camera)(void* scene_context, uint64_t entity_id, const LunaScriptCameraDesc* camera);
    int (*entity_set_camera_primary)(void* scene_context, uint64_t entity_id, int32_t primary);
    int (*entity_set_perspective_camera)(void* scene_context,
                                         uint64_t entity_id,
                                         float vertical_fov_radians,
                                         float near_clip,
                                         float far_clip);
    int (*entity_set_orthographic_camera)(void* scene_context,
                                          uint64_t entity_id,
                                          float vertical_size,
                                          float near_clip,
                                          float far_clip);

    // Input
    int (*input_is_key_pressed)(int32_t key_code);
    int (*input_is_mouse_button_pressed)(int32_t button_code);
    float (*input_get_mouse_x)();
    float (*input_get_mouse_y)();
    float (*input_get_mouse_delta_x)();
    float (*input_get_mouse_delta_y)();
    float (*input_get_mouse_scroll_x)();
    float (*input_get_mouse_scroll_y)();
    void (*input_set_cursor_mode)(int32_t cursor_mode);
    int32_t (*input_get_cursor_mode)();
    void (*input_set_mouse_position)(float x, float y);
    void (*input_set_raw_mouse_motion)(int32_t enabled);
} LunaScriptHostApi;

#if defined(__cplusplus)
}
#endif
