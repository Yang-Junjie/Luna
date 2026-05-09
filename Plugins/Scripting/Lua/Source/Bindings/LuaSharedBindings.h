#pragma once

#include <sol/sol.hpp>

namespace lua_plugin {

void bindLuaVec3Type(sol::state& lua_state);
void bindLuaCameraProjectionConstants(sol::state& lua_state);
void bindLuaInputConstants(sol::state& lua_state);
void bindLuaSchemaInspectionTypes(sol::state& lua_state);

} // namespace lua_plugin
