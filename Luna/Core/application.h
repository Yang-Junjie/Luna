#pragma once
#include "Events/ApplicationEvent.h"
#include "Events/Event.h"
#include "Imgui/ImGuiLayer.hpp"
#include "JobSystem/TaskSystem.h"
#include "layer.h"
#include "LayerStack.h"
#include "Renderer/Renderer.h"
#include "Timestep.h"
#include "window.h"

#include <memory>
#include <string>

namespace luna {

struct ApplicationSpecification {
    std::string m_name{"Luna"};
    uint32_t m_window_width{1'600}, m_window_height{900};
    bool m_maximized{false};
    bool m_enable_imgui{true};
    bool m_enable_multi_viewport{false};
};

class Application {
public:
    explicit Application(const ApplicationSpecification& spec);
    virtual ~Application();

    bool initialize();
    bool isInitialized() const;

    void close();
    void run();

    void pushLayer(std::unique_ptr<Layer> layer);
    void pushOverlay(std::unique_ptr<Layer> overlay);

    ImGuiLayer* getImGuiLayer() const;
    Timestep getTimestep() const;
    static Application& get();

    Renderer& getRenderer();
    TaskSystem& getTaskSystem();
    const TaskSystem& getTaskSystem() const;

protected:
    virtual Renderer::InitializationOptions getRendererInitializationOptions();

    virtual void onInit();
    virtual void onUpdate(Timestep);
    virtual void onShutdown();

    bool enableImGui(bool enable_multi_viewport = false);

private:
    void onEvent(Event& event);
    bool onWindowResize(const WindowResizeEvent& event);
    bool onWindowClose(const WindowCloseEvent& event);
    void renderFrame();

private:
    bool m_initialized{false};
    bool m_running{true};
    bool m_minimized{false};

    ApplicationSpecification m_specification;
    std::unique_ptr<Window> m_window;
    Renderer m_renderer;
    TaskSystem m_task_system;

    std::unique_ptr<ImGuiLayer> m_imgui_layer;
    ImGuiLayer* m_imgui_layer_raw{nullptr};
    LayerStack m_layer_stack;

    Timestep m_timestep;
    float m_last_frame_time{0.0f};
    static Application* m_s_instance;
};

Application* createApplication(int argc, char** argv);
} // namespace luna
