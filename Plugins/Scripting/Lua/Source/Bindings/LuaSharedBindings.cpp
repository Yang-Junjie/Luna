#include "LuaSharedBindings.h"

#include "Script/ScriptHostApi.h"

#include <glm/vec3.hpp>

#include <cstddef>

namespace {

struct LuaConstant {
    const char* name;
    int value;
};

template <size_t ConstantCount>
void bindConstantTable(sol::state& lua_state, const char* table_name, const LuaConstant (&constants)[ConstantCount])
{
    sol::table table = lua_state.create_named_table(table_name);
    for (const LuaConstant& constant : constants) {
        table[constant.name] = constant.value;
    }
}

} // namespace

namespace lua_plugin {

void bindLuaVec3Type(sol::state& lua_state)
{
    lua_state.new_usertype<glm::vec3>("Vec3",
                                      sol::call_constructor,
                                      sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
                                      "x",
                                      &glm::vec3::x,
                                      "y",
                                      &glm::vec3::y,
                                      "z",
                                      &glm::vec3::z);
}

void bindLuaCameraProjectionConstants(sol::state& lua_state)
{
    constexpr LuaConstant kCameraProjectionConstants[] = {
        {"Perspective", static_cast<int>(LunaScriptCameraProjectionType_Perspective)},
        {"Orthographic", static_cast<int>(LunaScriptCameraProjectionType_Orthographic)},
    };
    bindConstantTable(lua_state, "CameraProjection", kCameraProjectionConstants);
}

void bindLuaInputConstants(sol::state& lua_state)
{
    constexpr LuaConstant kKeyCodeConstants[] = {
        {"None", static_cast<int>(LunaScriptKeyCode_None)},
        {"LeftShift", static_cast<int>(LunaScriptKeyCode_LeftShift)},
        {"RightShift", static_cast<int>(LunaScriptKeyCode_RightShift)},
        {"LeftControl", static_cast<int>(LunaScriptKeyCode_LeftControl)},
        {"RightControl", static_cast<int>(LunaScriptKeyCode_RightControl)},
        {"LeftAlt", static_cast<int>(LunaScriptKeyCode_LeftAlt)},
        {"RightAlt", static_cast<int>(LunaScriptKeyCode_RightAlt)},
        {"Space", static_cast<int>(LunaScriptKeyCode_Space)},
        {"Enter", static_cast<int>(LunaScriptKeyCode_Enter)},
        {"Delete", static_cast<int>(LunaScriptKeyCode_Delete)},
        {"Escape", static_cast<int>(LunaScriptKeyCode_Escape)},
        {"Up", static_cast<int>(LunaScriptKeyCode_Up)},
        {"Down", static_cast<int>(LunaScriptKeyCode_Down)},
        {"Left", static_cast<int>(LunaScriptKeyCode_Left)},
        {"Right", static_cast<int>(LunaScriptKeyCode_Right)},
        {"Backspace", static_cast<int>(LunaScriptKeyCode_Backspace)},
        {"A", static_cast<int>(LunaScriptKeyCode_A)},
        {"B", static_cast<int>(LunaScriptKeyCode_B)},
        {"C", static_cast<int>(LunaScriptKeyCode_C)},
        {"D", static_cast<int>(LunaScriptKeyCode_D)},
        {"E", static_cast<int>(LunaScriptKeyCode_E)},
        {"F", static_cast<int>(LunaScriptKeyCode_F)},
        {"G", static_cast<int>(LunaScriptKeyCode_G)},
        {"H", static_cast<int>(LunaScriptKeyCode_H)},
        {"I", static_cast<int>(LunaScriptKeyCode_I)},
        {"J", static_cast<int>(LunaScriptKeyCode_J)},
        {"K", static_cast<int>(LunaScriptKeyCode_K)},
        {"L", static_cast<int>(LunaScriptKeyCode_L)},
        {"M", static_cast<int>(LunaScriptKeyCode_M)},
        {"N", static_cast<int>(LunaScriptKeyCode_N)},
        {"O", static_cast<int>(LunaScriptKeyCode_O)},
        {"P", static_cast<int>(LunaScriptKeyCode_P)},
        {"Q", static_cast<int>(LunaScriptKeyCode_Q)},
        {"R", static_cast<int>(LunaScriptKeyCode_R)},
        {"S", static_cast<int>(LunaScriptKeyCode_S)},
        {"T", static_cast<int>(LunaScriptKeyCode_T)},
        {"U", static_cast<int>(LunaScriptKeyCode_U)},
        {"V", static_cast<int>(LunaScriptKeyCode_V)},
        {"W", static_cast<int>(LunaScriptKeyCode_W)},
        {"X", static_cast<int>(LunaScriptKeyCode_X)},
        {"Y", static_cast<int>(LunaScriptKeyCode_Y)},
        {"Z", static_cast<int>(LunaScriptKeyCode_Z)},
    };
    bindConstantTable(lua_state, "KeyCode", kKeyCodeConstants);

    constexpr LuaConstant kMouseCodeConstants[] = {
        {"None", static_cast<int>(LunaScriptMouseCode_None)},
        {"Left", static_cast<int>(LunaScriptMouseCode_Left)},
        {"Right", static_cast<int>(LunaScriptMouseCode_Right)},
        {"Middle", static_cast<int>(LunaScriptMouseCode_Middle)},
        {"XButton1", static_cast<int>(LunaScriptMouseCode_XButton1)},
        {"XButton2", static_cast<int>(LunaScriptMouseCode_XButton2)},
    };
    bindConstantTable(lua_state, "MouseCode", kMouseCodeConstants);

    constexpr LuaConstant kCursorModeConstants[] = {
        {"Normal", static_cast<int>(LunaScriptCursorMode_Normal)},
        {"Hidden", static_cast<int>(LunaScriptCursorMode_Hidden)},
        {"Locked", static_cast<int>(LunaScriptCursorMode_Locked)},
    };
    bindConstantTable(lua_state, "CursorMode", kCursorModeConstants);
}

void bindLuaSchemaInspectionTypes(sol::state& lua_state)
{
    bindLuaVec3Type(lua_state);
    bindLuaInputConstants(lua_state);
    bindLuaCameraProjectionConstants(lua_state);
}

} // namespace lua_plugin
