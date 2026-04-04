#include "Core/log.h"
#include "Events/application_event.h"
#include "Events/key_event.h"
#include "Events/mouse_event.h"
#include "Platform/GLFWKeyCodes.hpp"
#include "Platform/GLFWMouseCodes.hpp"
#include "Platform/GLFWWindow.hpp"

namespace luna {

namespace {

uint32_t s_glfwWindowCount = 0;
GLFWwindow* s_activeWindow = nullptr;

void glfwErrorCallback(int error, const char* description)
{
    LUNA_CORE_ERROR("GLFW error {}: {}", error, description != nullptr ? description : "Unknown error");
}

} // namespace

GLFWWindow::GLFWWindow(const WindowProps& props)
{
    init(props);
}

GLFWWindow::~GLFWWindow()
{
    shutdown();
}

GLFWwindow* GLFWWindow::getActiveNativeWindow()
{
    return s_activeWindow;
}

void GLFWWindow::onUpdate()
{
    glfwPollEvents();
}

void GLFWWindow::getWindowPos(int* x, int* y) const
{
    if (m_window == nullptr) {
        return;
    }

    glfwGetWindowPos(m_window, x, y);
}

void GLFWWindow::setWindowPos(int x, int y)
{
    if (m_window == nullptr) {
        return;
    }

    glfwSetWindowPos(m_window, x, y);
}

void GLFWWindow::setMaximized()
{
    if (m_window == nullptr) {
        return;
    }

    glfwMaximizeWindow(m_window);
}

void GLFWWindow::setRestored()
{
    if (m_window == nullptr) {
        return;
    }

    glfwRestoreWindow(m_window);
}

void GLFWWindow::setMinimized()
{
    if (m_window == nullptr) {
        return;
    }

    glfwIconifyWindow(m_window);
}

void GLFWWindow::setVSync(bool enabled)
{
    m_data.vSync = enabled;
}

bool GLFWWindow::isVSync() const
{
    return m_data.vSync;
}

void GLFWWindow::init(const WindowProps& props)
{
    m_data.title = props.title;
    m_data.width = props.width;
    m_data.height = props.height;
    m_data.vSync = false;

    if (s_glfwWindowCount == 0) {
        glfwSetErrorCallback(glfwErrorCallback);
        if (!glfwInit()) {
            LUNA_CORE_ERROR("Failed to initialize GLFW");
            return;
        }
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, props.maximized ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, props.visible ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(
        static_cast<int>(props.width), static_cast<int>(props.height), props.title.c_str(), nullptr, nullptr);
    if (m_window == nullptr) {
        if (s_glfwWindowCount == 0) {
            glfwTerminate();
        }

        LUNA_CORE_ERROR("Failed to create GLFW window '{}'", props.title);
        return;
    }

    ++s_glfwWindowCount;
    s_activeWindow = m_window;

    int width = 0;
    int height = 0;
    glfwGetWindowSize(m_window, &width, &height);
    m_data.width = static_cast<uint32_t>(width);
    m_data.height = static_cast<uint32_t>(height);

    glfwSetWindowUserPointer(m_window, &m_data);

    glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        data.width = static_cast<uint32_t>(width);
        data.height = static_cast<uint32_t>(height);

        if (data.eventCallback) {
            WindowResizeEvent event(data.width, data.height);
            data.eventCallback(event);
        }
    });

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (data.eventCallback) {
            WindowCloseEvent event;
            data.eventCallback(event);
        }
    });

    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int, int action, int) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.eventCallback) {
            return;
        }

        const KeyCode lunaKey = GLFWKeyCodeToLunaKeyCode(key);
        switch (action) {
            case GLFW_PRESS: {
                KeyPressedEvent event(lunaKey, false);
                data.eventCallback(event);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent event(lunaKey, true);
                data.eventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent event(lunaKey);
                data.eventCallback(event);
                break;
            }
            default:
                break;
        }
    });

    glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.eventCallback) {
            return;
        }

        const KeyCode lunaKey = GLFWCharCodeToLunaKeyCode(codepoint);
        if (lunaKey == KeyCode::None) {
            return;
        }

        KeyTypedEvent event(lunaKey);
        data.eventCallback(event);
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.eventCallback) {
            return;
        }

        const MouseCode lunaButton = GLFWMouseCodeToLunaMouseCode(button);
        switch (action) {
            case GLFW_PRESS: {
                MouseButtonPressedEvent event(lunaButton);
                data.eventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                MouseButtonReleasedEvent event(lunaButton);
                data.eventCallback(event);
                break;
            }
            default:
                break;
        }
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.eventCallback) {
            return;
        }

        MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
        data.eventCallback(event);
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.eventCallback) {
            return;
        }

        MouseMovedEvent event(static_cast<float>(xPos), static_cast<float>(yPos));
        data.eventCallback(event);
    });

    LUNA_CORE_INFO("Created GLFW window '{}' ({}x{})", m_data.title, m_data.width, m_data.height);
}

void GLFWWindow::shutdown()
{
    if (m_window == nullptr) {
        return;
    }

    LUNA_CORE_INFO("Destroying GLFW window '{}'", m_data.title);

    if (s_activeWindow == m_window) {
        s_activeWindow = nullptr;
    }

    glfwDestroyWindow(m_window);
    m_window = nullptr;

    if (s_glfwWindowCount > 0) {
        --s_glfwWindowCount;
    }

    if (s_glfwWindowCount == 0) {
        glfwTerminate();
    }
}

} // namespace luna
