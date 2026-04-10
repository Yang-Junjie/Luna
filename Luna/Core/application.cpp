#include "Application.h"
#include "Core/Log.h"

#include <chrono>
#include <GLFW/glfw3.h>
#include <thread>

namespace luna {

Application* Application::m_s_instance = nullptr;

Application::Application(const ApplicationSpecification& spec)
    : m_specification(spec)
{
    if (m_s_instance != nullptr) {
        LUNA_CORE_ERROR("Application already exists");
        return;
    }

    m_s_instance = this;
}

Application::~Application()
{
    if (m_imgui_layer != nullptr) {
        m_imgui_layer->onDetach();
        m_imgui_layer.reset();
        m_imgui_layer_raw = nullptr;
    }

    m_task_system.shutdown();
    m_renderer.shutdown();
    m_window.reset();
    m_initialized = false;
    m_s_instance = nullptr;
}

bool Application::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (m_s_instance != this) {
        LUNA_CORE_ERROR("Cannot initialize application because another instance already exists");
        return false;
    }

    if (!m_task_system.initialize()) {
        LUNA_CORE_ERROR("Task system initialization failed");
        return false;
    }

    m_window = Window::create(WindowProps{m_specification.m_name,
                                          m_specification.m_window_width,
                                          m_specification.m_window_height,
                                          m_specification.m_maximized});

    if (m_window == nullptr) {
        LUNA_CORE_ERROR("Application window creation failed");
        return false;
    }

    m_window->setEventCallback([this](Event& event) {
        onEvent(event);
    });

    if (!m_renderer.init(*m_window, getRendererInitializationOptions())) {
        LUNA_CORE_ERROR("Renderer initialization failed");
        m_window.reset();
        return false;
    }

    m_imgui_layer = std::make_unique<ImGuiLayer>(m_renderer, m_specification.m_enable_multi_viewport);
    m_imgui_layer_raw = m_imgui_layer.get();
    m_imgui_layer->onAttach();

    if (!m_imgui_layer->isInitialized()) {
        LUNA_CORE_ERROR("ImGui initialization failed");
        m_imgui_layer.reset();
        m_imgui_layer_raw = nullptr;
        m_renderer.shutdown();
        m_window.reset();
        return false;
    }

    m_initialized = true;
    m_minimized = false;
    m_last_frame_time = 0.0f;

    return true;
}

void Application::run()
{
    if (!m_initialized && !initialize()) {
        LUNA_CORE_ERROR("Cannot run application because initialization failed");
        return;
    }

    m_running = true;
    m_last_frame_time = static_cast<float>(glfwGetTime());

    onInit();

    auto* native_window = static_cast<GLFWwindow*>(m_window->getNativeWindow());
    while (m_running && native_window != nullptr && !glfwWindowShouldClose(native_window)) {
        const float time = static_cast<float>(glfwGetTime());
        m_timestep = time - m_last_frame_time;
        m_last_frame_time = time;

        if (m_minimized) {
            m_task_system.waitForAll();
            m_window->onUpdate();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        onUpdate(m_timestep);

        for (auto& layer : m_layer_stack) {
            layer->onUpdate(m_timestep);
        }

        m_task_system.waitForAll();

        renderFrame();
        m_window->onUpdate();
    }

    m_running = false;
    onShutdown();
}

void Application::renderFrame()
{
    if (m_imgui_layer != nullptr && m_imgui_layer->isInitialized()) {
        m_imgui_layer->begin();

        for (auto& layer : m_layer_stack) {
            layer->onImGuiRender();
        }

        m_imgui_layer->end();
    }

    if (!m_renderer.isRenderingEnabled()) {
        return;
    }

    m_renderer.startFrame();

    for (auto& layer : m_layer_stack) {
        layer->onRender();
    }

    m_renderer.renderFrame();

    if (m_imgui_layer != nullptr) {
        m_imgui_layer->renderPlatformWindows();
    }

    m_renderer.endFrame();
}

void Application::pushLayer(std::unique_ptr<Layer> layer)
{
    Layer* raw_layer = layer.get();
    m_layer_stack.pushLayer(std::move(layer));
    raw_layer->onAttach();
}

void Application::pushOverlay(std::unique_ptr<Layer> overlay)
{
    Layer* raw_overlay = overlay.get();
    m_layer_stack.pushOverlay(std::move(overlay));
    raw_overlay->onAttach();
}

void Application::onEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
        return onWindowClose(e);
    });
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
        return onWindowResize(e);
    });

    if (m_imgui_layer != nullptr) {
        m_imgui_layer->onEvent(event);
    }

    for (auto it = m_layer_stack.rbegin(); it != m_layer_stack.rend(); ++it) {
        if (event.m_handled) {
            break;
        }

        (*it)->onEvent(event);
    }
}

bool Application::onWindowResize(const WindowResizeEvent& event)
{
    m_minimized = event.getWidth() == 0 || event.getHeight() == 0;
    if (!m_minimized) {
        m_renderer.requestResize();
    }
    return false;
}

bool Application::onWindowClose(const WindowCloseEvent&)
{
    m_running = false;

    if (m_window != nullptr) {
        auto* native_window = static_cast<GLFWwindow*>(m_window->getNativeWindow());
        if (native_window != nullptr) {
            glfwSetWindowShouldClose(native_window, GLFW_TRUE);
        }
    }

    return true;
}

} // namespace luna
