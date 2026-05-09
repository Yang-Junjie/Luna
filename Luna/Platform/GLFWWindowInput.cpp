#include "Core/Input.h"
#include "Platform/GLFWKeyCodes.hpp"
#include "Platform/GLFWMouseCodes.hpp"
#include "Platform/GLFWWindow.hpp"

#include <GLFW/glfw3.h>

namespace luna {

namespace {

GLFWwindow* getActiveWindow()
{
    return GLFWWindow::getActiveNativeWindow();
}

glm::vec2 s_mouse_delta{0.0f};
glm::vec2 s_mouse_scroll{0.0f};
glm::vec2 s_last_mouse_position{0.0f};
bool s_has_last_mouse_position = false;
bool s_ignore_next_mouse_move = false;
CursorMode s_cursor_mode = CursorMode::Normal;

int convertCursorModeToGLFW(CursorMode mode)
{
    switch (mode) {
        case CursorMode::Normal:
            return GLFW_CURSOR_NORMAL;
        case CursorMode::Hidden:
            return GLFW_CURSOR_HIDDEN;
        case CursorMode::Locked:
            return GLFW_CURSOR_DISABLED;
        default:
            return GLFW_CURSOR_NORMAL;
    }
}

} // namespace

bool Input::isKeyPressed(KeyCode key)
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return false;
    }

    const int state = glfwGetKey(window, lunaKeyCodeToGlfwKeyCode(key));
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::isMouseButtonPressed(MouseCode button)
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return false;
    }

    return glfwGetMouseButton(window, lunaMouseCodeToGlfwMouseCode(button)) == GLFW_PRESS;
}

glm::vec2 Input::getMousePosition()
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return {0.0f, 0.0f};
    }

    double x_pos = 0.0;
    double y_pos = 0.0;
    glfwGetCursorPos(window, &x_pos, &y_pos);

    return {static_cast<float>(x_pos), static_cast<float>(y_pos)};
}

glm::vec2 Input::getMouseDelta()
{
    return s_mouse_delta;
}

glm::vec2 Input::getMouseScrollOffset()
{
    return s_mouse_scroll;
}

void Input::setCursorMode(CursorMode mode)
{
    const bool mode_changed = mode != s_cursor_mode;
    s_cursor_mode = mode;

    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        if (mode_changed) {
            s_has_last_mouse_position = false;
            s_ignore_next_mouse_move = true;
            s_mouse_delta = glm::vec2(0.0f);
        }
        return;
    }

    glfwSetInputMode(window, GLFW_CURSOR, convertCursorModeToGLFW(mode));
    if (!mode_changed) {
        return;
    }

    double x_pos = 0.0;
    double y_pos = 0.0;
    glfwGetCursorPos(window, &x_pos, &y_pos);
    s_last_mouse_position = {static_cast<float>(x_pos), static_cast<float>(y_pos)};
    s_has_last_mouse_position = true;
    s_ignore_next_mouse_move = true;
    s_mouse_delta = glm::vec2(0.0f);
}

CursorMode Input::getCursorMode()
{
    return s_cursor_mode;
}

void Input::setMousePosition(float x, float y)
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return;
    }

    s_last_mouse_position = {x, y};
    s_has_last_mouse_position = true;
    glfwSetCursorPos(window, static_cast<double>(x), static_cast<double>(y));
}

void Input::setRawMouseMotion(bool enabled)
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr || !glfwRawMouseMotionSupported()) {
        return;
    }

    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, enabled ? GLFW_TRUE : GLFW_FALSE);
}

float Input::getMouseX()
{
    return getMousePosition().x;
}

float Input::getMouseY()
{
    return getMousePosition().y;
}

float Input::getMouseDeltaX()
{
    return getMouseDelta().x;
}

float Input::getMouseDeltaY()
{
    return getMouseDelta().y;
}

float Input::getMouseScrollX()
{
    return getMouseScrollOffset().x;
}

float Input::getMouseScrollY()
{
    return getMouseScrollOffset().y;
}

void Input::resetFrameState()
{
    s_mouse_delta = glm::vec2(0.0f);
    s_mouse_scroll = glm::vec2(0.0f);
}

void Input::recordMouseMoved(float x, float y)
{
    const glm::vec2 position{x, y};
    if (s_ignore_next_mouse_move) {
        s_last_mouse_position = position;
        s_has_last_mouse_position = true;
        s_ignore_next_mouse_move = false;
        return;
    }

    if (s_has_last_mouse_position) {
        s_mouse_delta += position - s_last_mouse_position;
    }

    s_last_mouse_position = position;
    s_has_last_mouse_position = true;
}

void Input::recordMouseScrolled(float x_offset, float y_offset)
{
    s_mouse_scroll += glm::vec2{x_offset, y_offset};
}

} // namespace luna
