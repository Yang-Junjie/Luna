#pragma once

#include "Script/ScriptHostApi.h"

#include <sol/sol.hpp>

namespace lua_plugin {

class LuaPluginRuntime;

void bindLuaSceneApi(LuaPluginRuntime& runtime);
void assignLuaScriptProperty(LuaPluginRuntime& runtime,
                             sol::table& instance_table,
                             const LunaScriptPropertyDesc& script_property);
void initializeLuaScriptInstanceTable(LuaPluginRuntime& runtime,
                                      sol::table& instance_table,
                                      const LunaScriptInstanceDesc& script_instance);

} // namespace lua_plugin
