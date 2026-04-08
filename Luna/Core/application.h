#pragma once
#include "Events/ApplicationEvent.h"
#include "Events/Event.h"
#include "Imgui/ImGuiLayer.hpp"
#include "Layer.h"
#include "LayerStack.h"
#include "Timestep.h"
#include "Vulkan/VkEngine.h"
#include "Window.h"

#include <memory>
#include <string>

namespace luna {

struct ApplicationSpecification {
    std::string m_name = "Luna";
    uint32_t m_window_width = 1'600, m_window_height = 900;
    bool m_maximized = false;
    bool m_enable_multi_viewport = false;
};

class Application {
public:
    explicit Application(const ApplicationSpecification& spec);
    virtual ~Application();

    void pushLayer(std::unique_ptr<Layer> layer);
    void pushOverlay(std::unique_ptr<Layer> overlay);

    ImGuiLayer* getImGuiLayer() const
    {
        return m_im_gui_layer_raw;
    }

    void close()
    {
        m_running = false;
    }

    void run();

    static Application& get()
    {
        return *m_s_instance;
    }

    Timestep getTimestep() const
    {
        return m_timestep;
    }

    bool isInitialized() const
    {
        return m_initialized;
    }

    // temporary
    VulkanEngine& getEngine()
    {
        return m_engine;
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

    std::unique_ptr<ImGuiLayer> m_im_gui_layer;
    ImGuiLayer* m_im_gui_layer_raw = nullptr;
    LayerStack m_layer_stack;

    Timestep m_timestep;
    float m_last_frame_time = 0.0f;
    static Application* m_s_instance;
};

Application* createApplication(int argc, char** argv);
} // namespace luna

