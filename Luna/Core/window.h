#pragma once
#include "Events/event.h"

#include <cstdint>

#include <functional>
#include <memory>
#include <string>

namespace luna {
struct WindowProps {
    std::string title;
    uint32_t width;
    uint32_t height;
    bool maximized = false;

    WindowProps(const std::string& title = "Luna",
                uint32_t width = 1'600,
                uint32_t height = 900,
                bool maximized = false)
        : title(title),
          width(width),
          height(height),
          maximized(maximized)
    {}
};

class Window {
public:
    using EventCallbackFn = std::function<void(Event&)>;

    virtual ~Window() = default;

    virtual void onUpdate() = 0;

    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;

    virtual void setEventCallback(const EventCallbackFn& callback) = 0;

    virtual void getWindowPos(int* x, int* y) const = 0;
    virtual void setWindowPos(int x, int y) = 0;

    virtual void setMaximized() = 0;
    virtual void setRestored() = 0;
    virtual void setMinimized() = 0;

    virtual void setVSync(bool enabled) = 0;
    virtual bool isVSync() const = 0;

    virtual void* getNativeWindow() const = 0;

    static std::unique_ptr<Window> create(const WindowProps& props = WindowProps());
};
} // namespace luna
