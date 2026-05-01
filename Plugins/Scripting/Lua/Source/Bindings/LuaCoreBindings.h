#pragma once

namespace lua_plugin {

class LuaPluginRuntime;

void bindLuaCoreApi(LuaPluginRuntime& runtime);
void updateLuaTimeApi(LuaPluginRuntime& runtime, float delta_time, float elapsed_time);

} // namespace lua_plugin
