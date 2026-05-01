#include "LuaPluginRuntime.h"

namespace {

struct LuaPluginBackendState {
    const LunaScriptHostApi* host_api{nullptr};
};

int createLuaRuntime(void* backend_user_data, LunaScriptRuntimeApi* out_runtime_api)
{
    if (backend_user_data == nullptr || out_runtime_api == nullptr) {
        return 0;
    }

    const auto* backend_state = static_cast<const LuaPluginBackendState*>(backend_user_data);
    return lua_plugin::createLuaRuntimeApi(backend_state->host_api, out_runtime_api);
}

} // namespace

extern "C" {

#if defined(_WIN32)
__declspec(dllexport)
#endif
int LunaCreateScriptPlugin(uint32_t host_api_version,
                           const LunaScriptHostApi* host_api,
                           LunaScriptPluginApi* out_plugin_api)
{
    if (host_api == nullptr || out_plugin_api == nullptr || host_api_version != LUNA_SCRIPT_HOST_API_VERSION) {
        return 0;
    }

    static LuaPluginBackendState backend_state{};
    static const char* supported_extensions[] = {".lua"};
    static LunaScriptBackendApi backend_api{};

    backend_state.host_api = host_api;

    backend_api.struct_size = sizeof(LunaScriptBackendApi);
    backend_api.api_version = LUNA_SCRIPT_BACKEND_API_VERSION;
    backend_api.backend_name = "Lua";
    backend_api.display_name = "Luna Lua";
    backend_api.language_name = "Lua";
    backend_api.supported_extensions = supported_extensions;
    backend_api.supported_extension_count = 1;
    backend_api.backend_user_data = &backend_state;
    backend_api.create_runtime = &createLuaRuntime;

    out_plugin_api->struct_size = sizeof(LunaScriptPluginApi);
    out_plugin_api->api_version = LUNA_SCRIPT_PLUGIN_API_VERSION;
    out_plugin_api->plugin_name = "Luna Lua";
    out_plugin_api->backends = &backend_api;
    out_plugin_api->backend_count = 1;
    return 1;
}

} // extern "C"
