#pragma once

#include "Core/Window.h"

#include <GLFW/glfw3.h>

namespace luna {

class GLFWWindow final : public Window {
public:
    explicit GLFWWindow(const WindowProps& props);
    ~GLFWWindow() override;

    void onUpdate() override;

    uint32_t getWidth() const override
    {
        return m_data.m_width;
    }

    uint32_t getHeight() const override
    {
        return m_data.m_height;
    }

    void setEventCallback(const EventCallbackFn& callback) override
    {
        m_data.m_event_callback = callback;
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
        std::string m_title;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        bool m_v_sync = false;
        EventCallbackFn m_event_callback;
    };

    GLFWwindow* m_window = nullptr;
    WindowData m_data;
};

} // namespace luna
