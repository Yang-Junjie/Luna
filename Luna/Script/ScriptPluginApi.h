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
