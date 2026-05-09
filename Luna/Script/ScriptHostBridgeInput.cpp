#include "ScriptHostBridgeInternal.h"

#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"

namespace {

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
