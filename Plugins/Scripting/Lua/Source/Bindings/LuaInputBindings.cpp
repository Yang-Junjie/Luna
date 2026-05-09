#include "LuaInputBindings.h"

#include "LuaSharedBindings.h"

#include "../LuaPluginRuntime.h"

#include <sol/sol.hpp>

#include <tuple>

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
    input.set_function("get_mouse_position", [host_api]() {
        const float x =
            host_api != nullptr && host_api->input_get_mouse_x != nullptr ? host_api->input_get_mouse_x() : 0.0f;
        const float y =
            host_api != nullptr && host_api->input_get_mouse_y != nullptr ? host_api->input_get_mouse_y() : 0.0f;
        return std::make_tuple(x, y);
    });
    input.set_function("get_mouse_delta_x", [host_api]() {
        return host_api != nullptr && host_api->input_get_mouse_delta_x != nullptr ? host_api->input_get_mouse_delta_x()
                                                                                   : 0.0f;
    });
    input.set_function("get_mouse_delta_y", [host_api]() {
        return host_api != nullptr && host_api->input_get_mouse_delta_y != nullptr ? host_api->input_get_mouse_delta_y()
                                                                                   : 0.0f;
    });
    input.set_function("get_mouse_delta", [host_api]() {
        const float x = host_api != nullptr && host_api->input_get_mouse_delta_x != nullptr
                            ? host_api->input_get_mouse_delta_x()
                            : 0.0f;
        const float y = host_api != nullptr && host_api->input_get_mouse_delta_y != nullptr
                            ? host_api->input_get_mouse_delta_y()
                            : 0.0f;
        return std::make_tuple(x, y);
    });
    input.set_function("get_mouse_scroll_x", [host_api]() {
        return host_api != nullptr && host_api->input_get_mouse_scroll_x != nullptr ? host_api->input_get_mouse_scroll_x()
                                                                                    : 0.0f;
    });
    input.set_function("get_mouse_scroll_y", [host_api]() {
        return host_api != nullptr && host_api->input_get_mouse_scroll_y != nullptr ? host_api->input_get_mouse_scroll_y()
                                                                                    : 0.0f;
    });
    input.set_function("get_mouse_scroll", [host_api]() {
        const float x = host_api != nullptr && host_api->input_get_mouse_scroll_x != nullptr
                            ? host_api->input_get_mouse_scroll_x()
                            : 0.0f;
        const float y = host_api != nullptr && host_api->input_get_mouse_scroll_y != nullptr
                            ? host_api->input_get_mouse_scroll_y()
                            : 0.0f;
        return std::make_tuple(x, y);
    });
    input.set_function("set_cursor_mode", [host_api](int cursor_mode) {
        if (host_api != nullptr && host_api->input_set_cursor_mode != nullptr) {
            host_api->input_set_cursor_mode(cursor_mode);
        }
    });
    input.set_function("get_cursor_mode", [host_api]() {
        return host_api != nullptr && host_api->input_get_cursor_mode != nullptr ? host_api->input_get_cursor_mode()
                                                                                 : 0;
    });
    input.set_function("set_mouse_position", [host_api](float x, float y) {
        if (host_api != nullptr && host_api->input_set_mouse_position != nullptr) {
            host_api->input_set_mouse_position(x, y);
        }
    });
    input.set_function("set_raw_mouse_motion", [host_api](bool enabled) {
        if (host_api != nullptr && host_api->input_set_raw_mouse_motion != nullptr) {
            host_api->input_set_raw_mouse_motion(enabled ? 1 : 0);
        }
    });

    bindLuaInputConstants(lua_state);
}

} // namespace lua_plugin
