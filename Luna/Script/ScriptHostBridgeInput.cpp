#include "ScriptHostBridgeInternal.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"

namespace {

static_assert(static_cast<int>(LunaScriptKeyCode_None) == static_cast<int>(luna::KeyCode::None));
static_assert(static_cast<int>(LunaScriptKeyCode_LeftShift) == static_cast<int>(luna::KeyCode::LeftShift));
static_assert(static_cast<int>(LunaScriptKeyCode_RightShift) == static_cast<int>(luna::KeyCode::RightShift));
static_assert(static_cast<int>(LunaScriptKeyCode_LeftControl) == static_cast<int>(luna::KeyCode::LeftControl));
static_assert(static_cast<int>(LunaScriptKeyCode_RightControl) == static_cast<int>(luna::KeyCode::RightControl));
static_assert(static_cast<int>(LunaScriptKeyCode_LeftAlt) == static_cast<int>(luna::KeyCode::LeftAlt));
static_assert(static_cast<int>(LunaScriptKeyCode_RightAlt) == static_cast<int>(luna::KeyCode::RightAlt));
static_assert(static_cast<int>(LunaScriptKeyCode_Space) == static_cast<int>(luna::KeyCode::Space));
static_assert(static_cast<int>(LunaScriptKeyCode_Enter) == static_cast<int>(luna::KeyCode::Enter));
static_assert(static_cast<int>(LunaScriptKeyCode_Delete) == static_cast<int>(luna::KeyCode::Delete));
static_assert(static_cast<int>(LunaScriptKeyCode_Escape) == static_cast<int>(luna::KeyCode::Escape));
static_assert(static_cast<int>(LunaScriptKeyCode_Up) == static_cast<int>(luna::KeyCode::Up));
static_assert(static_cast<int>(LunaScriptKeyCode_Down) == static_cast<int>(luna::KeyCode::Down));
static_assert(static_cast<int>(LunaScriptKeyCode_Left) == static_cast<int>(luna::KeyCode::Left));
static_assert(static_cast<int>(LunaScriptKeyCode_Right) == static_cast<int>(luna::KeyCode::Right));
static_assert(static_cast<int>(LunaScriptKeyCode_A) == static_cast<int>(luna::KeyCode::A));
static_assert(static_cast<int>(LunaScriptKeyCode_B) == static_cast<int>(luna::KeyCode::B));
static_assert(static_cast<int>(LunaScriptKeyCode_C) == static_cast<int>(luna::KeyCode::C));
static_assert(static_cast<int>(LunaScriptKeyCode_D) == static_cast<int>(luna::KeyCode::D));
static_assert(static_cast<int>(LunaScriptKeyCode_E) == static_cast<int>(luna::KeyCode::E));
static_assert(static_cast<int>(LunaScriptKeyCode_F) == static_cast<int>(luna::KeyCode::F));
static_assert(static_cast<int>(LunaScriptKeyCode_G) == static_cast<int>(luna::KeyCode::G));
static_assert(static_cast<int>(LunaScriptKeyCode_H) == static_cast<int>(luna::KeyCode::H));
static_assert(static_cast<int>(LunaScriptKeyCode_I) == static_cast<int>(luna::KeyCode::I));
static_assert(static_cast<int>(LunaScriptKeyCode_J) == static_cast<int>(luna::KeyCode::J));
static_assert(static_cast<int>(LunaScriptKeyCode_K) == static_cast<int>(luna::KeyCode::K));
static_assert(static_cast<int>(LunaScriptKeyCode_L) == static_cast<int>(luna::KeyCode::L));
static_assert(static_cast<int>(LunaScriptKeyCode_M) == static_cast<int>(luna::KeyCode::M));
static_assert(static_cast<int>(LunaScriptKeyCode_N) == static_cast<int>(luna::KeyCode::N));
static_assert(static_cast<int>(LunaScriptKeyCode_O) == static_cast<int>(luna::KeyCode::O));
static_assert(static_cast<int>(LunaScriptKeyCode_P) == static_cast<int>(luna::KeyCode::P));
static_assert(static_cast<int>(LunaScriptKeyCode_Q) == static_cast<int>(luna::KeyCode::Q));
static_assert(static_cast<int>(LunaScriptKeyCode_R) == static_cast<int>(luna::KeyCode::R));
static_assert(static_cast<int>(LunaScriptKeyCode_S) == static_cast<int>(luna::KeyCode::S));
static_assert(static_cast<int>(LunaScriptKeyCode_T) == static_cast<int>(luna::KeyCode::T));
static_assert(static_cast<int>(LunaScriptKeyCode_U) == static_cast<int>(luna::KeyCode::U));
static_assert(static_cast<int>(LunaScriptKeyCode_V) == static_cast<int>(luna::KeyCode::V));
static_assert(static_cast<int>(LunaScriptKeyCode_W) == static_cast<int>(luna::KeyCode::W));
static_assert(static_cast<int>(LunaScriptKeyCode_X) == static_cast<int>(luna::KeyCode::X));
static_assert(static_cast<int>(LunaScriptKeyCode_Y) == static_cast<int>(luna::KeyCode::Y));
static_assert(static_cast<int>(LunaScriptKeyCode_Z) == static_cast<int>(luna::KeyCode::Z));

static_assert(static_cast<int>(LunaScriptMouseCode_None) == static_cast<int>(luna::MouseCode::None));
static_assert(static_cast<int>(LunaScriptMouseCode_Left) == static_cast<int>(luna::MouseCode::Left));
static_assert(static_cast<int>(LunaScriptMouseCode_Right) == static_cast<int>(luna::MouseCode::Right));
static_assert(static_cast<int>(LunaScriptMouseCode_Middle) == static_cast<int>(luna::MouseCode::Middle));
static_assert(static_cast<int>(LunaScriptMouseCode_XButton1) == static_cast<int>(luna::MouseCode::XButton1));
static_assert(static_cast<int>(LunaScriptMouseCode_XButton2) == static_cast<int>(luna::MouseCode::XButton2));

static_assert(static_cast<int>(LunaScriptCursorMode_Normal) == static_cast<int>(luna::CursorMode::Normal));
static_assert(static_cast<int>(LunaScriptCursorMode_Hidden) == static_cast<int>(luna::CursorMode::Hidden));
static_assert(static_cast<int>(LunaScriptCursorMode_Locked) == static_cast<int>(luna::CursorMode::Locked));

int inputIsKeyPressed(int32_t key_code)
{
    return luna::Input::isKeyPressed(static_cast<luna::KeyCode>(key_code)) ? 1 : 0;
}

int inputIsMouseButtonPressed(int32_t button_code)
{
    return luna::Input::isMouseButtonPressed(static_cast<luna::MouseCode>(button_code)) ? 1 : 0;
}

float inputGetMouseX()
{
    return luna::Input::getMouseX();
}

float inputGetMouseY()
{
    return luna::Input::getMouseY();
}

float inputGetMouseDeltaX()
{
    return luna::Input::getMouseDeltaX();
}

float inputGetMouseDeltaY()
{
    return luna::Input::getMouseDeltaY();
}

float inputGetMouseScrollX()
{
    return luna::Input::getMouseScrollX();
}

float inputGetMouseScrollY()
{
    return luna::Input::getMouseScrollY();
}

void inputSetCursorMode(int32_t cursor_mode)
{
    luna::Input::setCursorMode(static_cast<luna::CursorMode>(cursor_mode));
}

int32_t inputGetCursorMode()
{
    return static_cast<int32_t>(luna::Input::getCursorMode());
}

void inputSetMousePosition(float x, float y)
{
    luna::Input::setMousePosition(x, y);
}

void inputSetRawMouseMotion(int32_t enabled)
{
    luna::Input::setRawMouseMotion(enabled != 0);
}

} // namespace

namespace luna {

void registerScriptInputHostApi(LunaScriptHostApi& host_api)
{
    host_api.input_is_key_pressed = &inputIsKeyPressed;
    host_api.input_is_mouse_button_pressed = &inputIsMouseButtonPressed;
    host_api.input_get_mouse_x = &inputGetMouseX;
    host_api.input_get_mouse_y = &inputGetMouseY;
    host_api.input_get_mouse_delta_x = &inputGetMouseDeltaX;
    host_api.input_get_mouse_delta_y = &inputGetMouseDeltaY;
    host_api.input_get_mouse_scroll_x = &inputGetMouseScrollX;
    host_api.input_get_mouse_scroll_y = &inputGetMouseScrollY;
    host_api.input_set_cursor_mode = &inputSetCursorMode;
    host_api.input_get_cursor_mode = &inputGetCursorMode;
    host_api.input_set_mouse_position = &inputSetMousePosition;
    host_api.input_set_raw_mouse_motion = &inputSetRawMouseMotion;
}

} // namespace luna
