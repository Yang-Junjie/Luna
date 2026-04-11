#include "Core/Log.h"
#include "Events/ApplicationEvent.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Platform/GLFWKeyCodes.hpp"
#include "Platform/GLFWMouseCodes.hpp"
#include "Platform/GLFWWindow.hpp"

namespace luna {

namespace {

uint32_t s_glfw_window_count = 0;
GLFWwindow* s_active_window = nullptr;

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
    return s_active_window;
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
    m_data.m_v_sync = enabled;
}

bool GLFWWindow::isVSync() const
{
    return m_data.m_v_sync;
}

void GLFWWindow::init(const WindowProps& props)
{
    m_data.m_title = props.m_title;
    m_data.m_width = props.m_width;
    m_data.m_height = props.m_height;
    m_data.m_v_sync = false;

    if (s_glfw_window_count == 0) {
        glfwSetErrorCallback(glfwErrorCallback);
        if (!glfwInit()) {
            LUNA_CORE_ERROR("Failed to initialize GLFW");
            return;
        }
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, props.m_maximized ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(
        static_cast<int>(props.m_width), static_cast<int>(props.m_height), props.m_title.c_str(), nullptr, nullptr);
    if (m_window == nullptr) {
        if (s_glfw_window_count == 0) {
            glfwTerminate();
        }

        LUNA_CORE_ERROR("Failed to create GLFW window '{}'", props.m_title);
        return;
    }

    ++s_glfw_window_count;
    s_active_window = m_window;

    int width = 0;
    int height = 0;
    glfwGetWindowSize(m_window, &width, &height);
    m_data.m_width = static_cast<uint32_t>(width);
    m_data.m_height = static_cast<uint32_t>(height);

    glfwSetWindowUserPointer(m_window, &m_data);

    glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        data.m_width = static_cast<uint32_t>(width);
        data.m_height = static_cast<uint32_t>(height);

        if (data.m_event_callback) {
            WindowResizeEvent event(data.m_width, data.m_height);
            data.m_event_callback(event);
        }
    });

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (data.m_event_callback) {
            WindowCloseEvent event;
            data.m_event_callback(event);
        }
    });

    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int, int action, int) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.m_event_callback) {
            return;
        }

        const KeyCode luna_key = glfwKeyCodeToLunaKeyCode(key);
        switch (action) {
            case GLFW_PRESS: {
                KeyPressedEvent event(luna_key, false);
                data.m_event_callback(event);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent event(luna_key, true);
                data.m_event_callback(event);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent event(luna_key);
                data.m_event_callback(event);
                break;
            }
            default:
                break;
        }
    });

    glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.m_event_callback) {
            return;
        }

        const KeyCode luna_key = glfwCharCodeToLunaKeyCode(codepoint);
        if (luna_key == KeyCode::None) {
            return;
        }

        KeyTypedEvent event(luna_key);
        data.m_event_callback(event);
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.m_event_callback) {
            return;
        }

        const MouseCode luna_button = glfwMouseCodeToLunaMouseCode(button);
        switch (action) {
            case GLFW_PRESS: {
                MouseButtonPressedEvent event(luna_button);
                data.m_event_callback(event);
                break;
            }
            case GLFW_RELEASE: {
                MouseButtonReleasedEvent event(luna_button);
                data.m_event_callback(event);
                break;
            }
            default:
                break;
        }
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double x_offset, double y_offset) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.m_event_callback) {
            return;
        }

        MouseScrolledEvent event(static_cast<float>(x_offset), static_cast<float>(y_offset));
        data.m_event_callback(event);
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x_pos, double y_pos) {
        auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
        if (!data.m_event_callback) {
            return;
        }

        MouseMovedEvent event(static_cast<float>(x_pos), static_cast<float>(y_pos));
        data.m_event_callback(event);
    });

    LUNA_CORE_INFO("Created GLFW window '{}' ({}x{})", m_data.m_title, m_data.m_width, m_data.m_height);
}

void GLFWWindow::shutdown()
{
    if (m_window == nullptr) {
        return;
    }

    LUNA_CORE_INFO("Destroying GLFW window '{}'", m_data.m_title);

    if (s_active_window == m_window) {
        s_active_window = nullptr;
    }

    glfwDestroyWindow(m_window);
    m_window = nullptr;

    if (s_glfw_window_count > 0) {
        --s_glfw_window_count;
    }

    if (s_glfw_window_count == 0) {
        glfwTerminate();
    }
}

} // namespace luna
