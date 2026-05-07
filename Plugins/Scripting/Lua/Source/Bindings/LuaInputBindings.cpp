#include "../LuaPluginRuntime.h"
#include "LuaInputBindings.h"

#include <sol/sol.hpp>

namespace {

void bindKeyCodes(sol::state& lua_state)
{
    sol::table key_code = lua_state.create_named_table("KeyCode");
    key_code["None"] = 0;
    key_code["LeftShift"] = 1;
    key_code["RightShift"] = 2;
    key_code["LeftControl"] = 3;
    key_code["RightControl"] = 4;
    key_code["LeftAlt"] = 5;
    key_code["RightAlt"] = 6;
    key_code["Space"] = 7;
    key_code["Enter"] = 8;
    key_code["Delete"] = 9;
    key_code["Escape"] = 10;
    key_code["Up"] = 11;
    key_code["Down"] = 12;
    key_code["Left"] = 13;
    key_code["Right"] = 14;
    key_code["A"] = 65;
    key_code["B"] = 66;
    key_code["C"] = 67;
    key_code["D"] = 68;
    key_code["E"] = 69;
    key_code["F"] = 70;
    key_code["G"] = 71;
    key_code["H"] = 72;
    key_code["I"] = 73;
    key_code["J"] = 74;
    key_code["K"] = 75;
    key_code["L"] = 76;
    key_code["M"] = 77;
    key_code["N"] = 78;
    key_code["O"] = 79;
    key_code["P"] = 80;
    key_code["Q"] = 81;
    key_code["R"] = 82;
    key_code["S"] = 83;
    key_code["T"] = 84;
    key_code["U"] = 85;
    key_code["V"] = 86;
    key_code["W"] = 87;
    key_code["X"] = 88;
    key_code["Y"] = 89;
    key_code["Z"] = 90;
}

void bindMouseCodes(sol::state& lua_state)
{
    sol::table mouse_code = lua_state.create_named_table("MouseCode");
    mouse_code["None"] = 0;
    mouse_code["Left"] = 1;
    mouse_code["Right"] = 2;
    mouse_code["Middle"] = 3;
    mouse_code["XButton1"] = 4;
    mouse_code["XButton2"] = 5;
}

} // namespace

namespace lua_plugin {

void bindLuaInputApi(LuaPluginRuntime& runtime)
{
    sol::state& lua_state = runtime.luaState();
    const LunaScriptHostApi* host_api = runtime.hostApi();

    sol::table input = lua_state.create_named_table("Input");
    input.set_function("is_key_pressed", [host_api](int key_code) {
        return host_api != nullptr && host_api->input_is_key_pressed != nullptr &&
               host_api->input_is_key_pressed(key_code) != 0;
    });
    input.set_function("is_mouse_button_pressed", [host_api](int button_code) {
        return host_api != nullptr && host_api->input_is_mouse_button_pressed != nullptr &&
               host_api->input_is_mouse_button_pressed(button_code) != 0;
    });
    input.set_function("get_mouse_x", [host_api]() {
        return host_api != nullptr && host_api->input_get_mouse_x != nullptr ? host_api->input_get_mouse_x() : 0.0f;
    });
    input.set_function("get_mouse_y", [host_api]() {
        return host_api != nullptr && host_api->input_get_mouse_y != nullptr ? host_api->input_get_mouse_y() : 0.0f;
    });

    bindKeyCodes(lua_state);
    bindMouseCodes(lua_state);
}

} // namespace lua_plugin
