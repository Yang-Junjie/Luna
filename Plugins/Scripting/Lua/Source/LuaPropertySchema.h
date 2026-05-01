#pragma once

#include "Script/ScriptHostApi.h"
#include "Script/ScriptPluginApi.h"

namespace lua_plugin {

int enumerateLuaPropertySchema(const LunaScriptHostApi* host_api,
                               const LunaScriptSchemaRequest* request,
                               void* user_data,
                               LunaScriptEnumeratePropertySchemaFn enumerate_fn);

} // namespace lua_plugin
