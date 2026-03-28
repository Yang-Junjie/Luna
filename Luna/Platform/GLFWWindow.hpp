#pragma once

#include "Core/window.h"

#include <GLFW/glfw3.h>

namespace luna {

class GLFWWindow final : public Window {
public:
    explicit GLFWWindow(const WindowProps& props);
    ~GLFWWindow() override;

    void onUpdate() override;

    uint32_t getWidth() const override
    {
        return m_data.width;
    }

    uint32_t getHeight() const override
    {
        return m_data.height;
    }

    void setEventCallback(const EventCallbackFn& callback) override
    {
        m_data.eventCallback = callback;
    }

    void getWindowPos(int* x, int* y) const override;
    void setWindowPos(int x, int y) override;

    void setMaximized() override;
    void setRestored() override;
    void setMinimized() override;

    void setVSync(bool enabled) override;
    bool isVSync() const override;

    void* getNativeWindow() const override
    {
        return m_window;
    }

    static GLFWwindow* getActiveNativeWindow();

private:
    void init(const WindowProps& props);
    void shutdown();

private:
    struct WindowData {
        std::string title;
        uint32_t width = 0;
        uint32_t height = 0;
        bool vSync = false;
        EventCallbackFn eventCallback;
    };

    GLFWwindow* m_window = nullptr;
    WindowData m_data;
};

} // namespace luna
