#pragma once

#include "ScriptHostApi.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    LUNA_SCRIPT_PLUGIN_API_VERSION = 1u,
    LUNA_SCRIPT_BACKEND_API_VERSION = 1u,
    LUNA_SCRIPT_RUNTIME_API_VERSION = 1u,
};

typedef struct LunaScriptSchemaRequest {
    const char* asset_name;
    const char* type_name;
    const char* language;
    const char* source;
} LunaScriptSchemaRequest;

typedef struct LunaScriptPropertySchemaDesc {
    const char* name;
    const char* display_name;
    const char* description;
    LunaScriptPropertyType type;
    int32_t default_bool_value;
    int32_t default_int_value;
    float default_float_value;
    const char* default_string_value;
    LunaScriptVec3 default_vec3_value;
    uint64_t default_entity_value;
    uint64_t default_asset_value;
} LunaScriptPropertySchemaDesc;

typedef struct LunaScriptPropertyValueDesc {
    const char* name;
    LunaScriptPropertyType type;
    int32_t bool_value;
    int32_t int_value;
    float float_value;
    const char* string_value;
    LunaScriptVec3 vec3_value;
    uint64_t entity_value;
    uint64_t asset_value;
    size_t property_index;
} LunaScriptPropertyValueDesc;

typedef int (*LunaScriptEnumeratePropertySchemaFn)(void* user_data,
                                                   const LunaScriptPropertySchemaDesc* property_schema);

typedef struct LunaScriptRuntimeApi {
    uint32_t struct_size;
    uint32_t api_version;
    void* runtime_user_data;
    void (*destroy_runtime)(void* runtime_user_data);
    int (*initialize)(void* runtime_user_data);
    void (*shutdown)(void* runtime_user_data);
    void (*on_runtime_start)(void* runtime_user_data, void* scene_context);
    void (*on_runtime_stop)(void* runtime_user_data, void* scene_context);
    void (*on_update)(void* runtime_user_data, void* scene_context, float delta_time_seconds);
    void (*set_script_property)(void* runtime_user_data,
                                void* scene_context,
                                uint64_t entity_id,
                                uint64_t script_id,
                                const LunaScriptPropertyValueDesc* property);
} LunaScriptRuntimeApi;

typedef struct LunaScriptBackendApi {
    uint32_t struct_size;
    uint32_t api_version;
    const char* backend_name;
    const char* display_name;
    const char* language_name;
    const char* const* supported_extensions;
    size_t supported_extension_count;
    void* backend_user_data;
    int (*create_runtime)(void* backend_user_data, LunaScriptRuntimeApi* out_runtime_api);
    int (*enumerate_property_schema)(void* backend_user_data,
                                     const LunaScriptSchemaRequest* request,
                                     void* user_data,
                                     LunaScriptEnumeratePropertySchemaFn enumerate_fn);
} LunaScriptBackendApi;

typedef struct LunaScriptPluginApi {
    uint32_t struct_size;
    uint32_t api_version;
    const char* plugin_name;
    const LunaScriptBackendApi* backends;
    size_t backend_count;
} LunaScriptPluginApi;

typedef int (*LunaCreateScriptPluginFn)(uint32_t host_api_version,
                                        const LunaScriptHostApi* host_api,
                                        LunaScriptPluginApi* out_plugin_api);

#if defined(__cplusplus)
}
#endif
