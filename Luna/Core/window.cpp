#include "Log.h"
#include "Platform/GLFWWindow.hpp"
#include "window.h"

namespace luna {
std::unique_ptr<Window> Window::create(const WindowProps& props)
{
    auto window = std::make_unique<GLFWWindow>(props);
    if (window->getNativeWindow() == nullptr) {
        LUNA_CORE_ERROR("Failed to create window '{}'", props.m_title);
        return nullptr;
    }

    return window;
}
} // namespace luna
