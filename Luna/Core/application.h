#pragma once
#include "Events/application_event.h"
#include "Events/event.h"
#include "layer.h"
#include "layer_stack.h"
#include "timestep.h"
#include "Vulkan/vk_engine.h"
#include "window.h"

#include <memory>
#include <string>

namespace luna {

struct ApplicationSpecification {
    std::string name = "Luna";
    uint32_t windowWidth = 1'600, windowHeight = 900;
    bool maximized = false;
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

protected:
    virtual void onInit() {}

    virtual void onUpdate(Timestep) {}

    virtual void onShutdown() {}

private:
    void onEvent(Event& event);
    bool onWindowResize(const WindowResizeEvent& event);
    bool onWindowClose(const WindowCloseEvent& event);
    void renderFrame();

private:
    bool m_initialized = false;
    bool m_running = true;
    bool m_minimized = false;

    ApplicationSpecification m_specification;
    std::unique_ptr<Window> m_window;
    VulkanEngine m_engine;
    LayerStack m_layerStack;

    Timestep m_timestep;
    float m_lastFrameTime = 0.0f;
    static Application* s_instance;
};

Application* createApplication(int argc, char** argv);
} // namespace luna
