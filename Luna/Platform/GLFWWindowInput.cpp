#include "Core/input.hpp"
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

    const int state = glfwGetKey(window, LunaKeyCodeToGLFWKeyCode(key));
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::isMouseButtonPressed(MouseCode button)
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return false;
    }

    return glfwGetMouseButton(window, LunaMouseCodeToGLFWMouseCode(button)) == GLFW_PRESS;
}

glm::vec2 Input::getMousePosition()
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return {0.0f, 0.0f};
    }

    double xPos = 0.0;
    double yPos = 0.0;
    glfwGetCursorPos(window, &xPos, &yPos);

    return {static_cast<float>(xPos), static_cast<float>(yPos)};
}

void Input::setCursorMode(CursorMode mode)
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return;
    }

    glfwSetInputMode(window, GLFW_CURSOR, convertCursorModeToGLFW(mode));
}

void Input::setMousePosition(float x, float y)
{
    GLFWwindow* window = getActiveWindow();
    if (window == nullptr) {
        return;
    }

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

} // namespace luna
