#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
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
    LunaScriptHostLogFn log;
    const char* (*scene_get_name)(void* scene_context);
    int (*scene_enumerate_script_instances)(void* scene_context,
                                            void* user_data,
                                            LunaScriptEnumerateInstancesFn enumerate_fn);
    int (*scene_enumerate_script_properties)(void* scene_context,
                                             uint64_t entity_id,
                                             uint64_t script_id,
                                             void* user_data,
                                             LunaScriptEnumeratePropertiesFn enumerate_fn);
    int (*entity_is_valid)(void* scene_context, uint64_t entity_id);
    const char* (*entity_get_name)(void* scene_context, uint64_t entity_id);
    int (*entity_get_translation)(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value);
    int (*entity_set_translation)(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value);
    int (*entity_get_rotation)(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value);
    int (*entity_set_rotation)(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value);
    int (*entity_get_scale)(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value);
    int (*entity_set_scale)(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value);
    int (*input_is_key_pressed)(int32_t key_code);
    int (*input_is_mouse_button_pressed)(int32_t button_code);
    float (*input_get_mouse_x)();
    float (*input_get_mouse_y)();
} LunaScriptHostApi;

#if defined(__cplusplus)
}
#endif
