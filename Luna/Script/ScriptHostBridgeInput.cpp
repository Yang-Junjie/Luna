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

} // namespace

namespace luna {

void registerScriptInputHostApi(LunaScriptHostApi& host_api)
{
    host_api.input_is_key_pressed = &inputIsKeyPressed;
    host_api.input_is_mouse_button_pressed = &inputIsMouseButtonPressed;
    host_api.input_get_mouse_x = &inputGetMouseX;
    host_api.input_get_mouse_y = &inputGetMouseY;
}

} // namespace luna
