#pragma once
#include "Events/application_event.h"
#include "Events/event.h"
#include "layer.h"
#include "layer_stack.h"
#include "timestep.h"
#include "window.h"

#include <array>
#include <memory>
#include <string>

namespace luna {

struct ApplicationSpecification {
    std::string name = "Luna";
    uint32_t windowWidth = 1'600, windowHeight = 900;
    bool maximized = false;
    bool enableImGui = true;
    bool enableMultiViewport = false;
};

class Application {
public:
    explicit Application(const ApplicationSpecification& spec);
    virtual ~Application();

    void pushLayer(std::unique_ptr<Layer> layer);
    void pushOverlay(std::unique_ptr<Layer> overlay);

    void close()
    {
        m_running = false;
    }

    void run();

    static Application& get()
    {
        return *s_instance;
    }

    Timestep getTimestep() const
    {
        return m_timestep;
    }

    bool isInitialized() const
    {
        return m_initialized;
    }

    Window& getWindow()
    {
        return *m_window;
    }

protected:
    virtual void onInit() {}
    virtual void onUpdate(Timestep) {}
    virtual void onShutdown() {}
    virtual void onEventReceived(Event&) {}
    virtual void onWindowResized(uint32_t, uint32_t) {}
    virtual void onWindowMinimized(bool) {}

    bool isMinimized() const
    {
        return m_minimized;
    }

    void failInitialization()
    {
        m_initialized = false;
        m_running = false;
    }

private:
    void handleEvent(Event& event);
    bool onWindowResize(const WindowResizeEvent& event);
    bool onWindowClose(const WindowCloseEvent& event);

private:
    bool m_initialized = false;
    bool m_running = true;
    bool m_minimized = false;

    ApplicationSpecification m_specification;
    std::unique_ptr<Window> m_window;

    LayerStack m_layerStack;

    Timestep m_timestep;
    float m_lastFrameTime = 0.0f;
    static Application* s_instance;
};

Application* createApplication(int argc, char** argv);
} // namespace luna
