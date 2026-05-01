#include "LuaCoreBindings.h"

#include "../LuaPluginRuntime.h"

#include <sol/sol.hpp>

namespace lua_plugin {

void bindLuaCoreApi(LuaPluginRuntime& runtime)
{
    sol::state& lua_state = runtime.luaState();

    sol::table log = lua_state.create_named_table("Log");
    log.set_function("info", [&runtime](const std::string& message) {
        runtime.log(LunaScriptHostLogLevel_Info, "[Lua] " + message);
    });
    log.set_function("warn", [&runtime](const std::string& message) {
        runtime.log(LunaScriptHostLogLevel_Warn, "[Lua] " + message);
    });
    log.set_function("error", [&runtime](const std::string& message) {
        runtime.log(LunaScriptHostLogLevel_Error, "[Lua] " + message);
    });
}

void updateLuaTimeApi(LuaPluginRuntime& runtime, float delta_time, float elapsed_time)
{
    sol::state& lua_state = runtime.luaState();
    sol::object time_object = lua_state["Time"];
    sol::table time =
        time_object.is<sol::table>() ? time_object.as<sol::table>() : lua_state.create_named_table("Time");

    time["delta_time"] = delta_time;
    time["elapsed_time"] = elapsed_time;
}

} // namespace lua_plugin
