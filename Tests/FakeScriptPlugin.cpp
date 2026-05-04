#include "Script/ScriptPluginApi.h"

#include <cstddef>

#if defined(_WIN32)
#    define LUNA_TEST_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#    define LUNA_TEST_PLUGIN_EXPORT extern "C"
#endif

namespace {

void destroyRuntime(void*) {}

int createRuntime(void*, LunaScriptRuntimeApi* out_runtime_api)
{
    if (out_runtime_api == nullptr) {
        return 0;
    }

    *out_runtime_api = {};
    out_runtime_api->struct_size = sizeof(LunaScriptRuntimeApi);
    out_runtime_api->api_version = LUNA_SCRIPT_RUNTIME_API_VERSION;
    out_runtime_api->destroy_runtime = &destroyRuntime;
    return 1;
}

int enumeratePropertySchema(void*,
                            const LunaScriptSchemaRequest*,
                            void*,
                            LunaScriptEnumeratePropertySchemaFn)
{
    return 1;
}

#if defined(LUNA_TEST_SCRIPT_PLUGIN_BACKEND_MISMATCH)
constexpr const char* kBackendName = "OtherBackend";
#else
constexpr const char* kBackendName = "ContractBackend";
#endif

#if defined(LUNA_TEST_SCRIPT_PLUGIN_LANGUAGE_MISMATCH)
constexpr const char* kLanguageName = "OtherLanguage";
#else
constexpr const char* kLanguageName = "ContractLanguage";
#endif

#if defined(LUNA_TEST_SCRIPT_PLUGIN_EXTENSION_MISMATCH)
constexpr const char* kSupportedExtensions[] = {".other"};
#elif defined(LUNA_TEST_SCRIPT_PLUGIN_EMPTY_EXTENSIONS)
constexpr const char* const* kSupportedExtensions = nullptr;
#else
constexpr const char* kSupportedExtensions[] = {".contract"};
#endif

#if defined(LUNA_TEST_SCRIPT_PLUGIN_EMPTY_EXTENSIONS)
constexpr std::size_t kSupportedExtensionCount = 0;
#else
constexpr std::size_t kSupportedExtensionCount = 1;
#endif

constexpr LunaScriptBackendApi kBackends[] = {
    {
        sizeof(LunaScriptBackendApi),
        LUNA_SCRIPT_BACKEND_API_VERSION,
        kBackendName,
        "Contract Test Backend",
        kLanguageName,
        kSupportedExtensions,
        kSupportedExtensionCount,
        nullptr,
        &createRuntime,
        &enumeratePropertySchema,
    },
};

} // namespace

LUNA_TEST_PLUGIN_EXPORT int LunaCreateScriptPlugin(uint32_t host_api_version,
                                                  const LunaScriptHostApi*,
                                                  LunaScriptPluginApi* out_plugin_api)
{
    if (host_api_version != LUNA_SCRIPT_HOST_API_VERSION || out_plugin_api == nullptr) {
        return 0;
    }

    *out_plugin_api = {};
    out_plugin_api->struct_size = sizeof(LunaScriptPluginApi);
    out_plugin_api->api_version = LUNA_SCRIPT_PLUGIN_API_VERSION;
    out_plugin_api->plugin_name = "Luna Contract Test Script Plugin";
    out_plugin_api->backends = kBackends;
    out_plugin_api->backend_count = 1;
    return 1;
}
