#include "application.h"
#include "log.h"

#include <chrono>
#include <GLFW/glfw3.h>
#include <thread>

namespace luna {

Application* Application::s_instance = nullptr;

Application::Application(const ApplicationSpecification& spec)
    : m_specification(spec)
{
    if (s_instance != nullptr) {
        LUNA_CORE_ERROR("Application already exists");
        return;
    }

    s_instance = this;
    m_window = Window::create(WindowProps{m_specification.name,
                                          m_specification.windowWidth,
                                          m_specification.windowHeight,
                                          m_specification.maximized,
                                          true});

    if (m_window == nullptr) {
        LUNA_CORE_ERROR("Application window creation failed");
        s_instance = nullptr;
        return;
    }

    m_window->setEventCallback([this](Event& event) {
        handleEvent(event);
    });

    m_initialized = true;
}

Application::~Application()
{
    s_instance = nullptr;
}

void Application::run()
{
    if (!m_initialized) {
        LUNA_CORE_ERROR("Cannot run application because initialization failed");
        return;
    }

    onInit();

    auto* nativeWindow = static_cast<GLFWwindow*>(m_window->getNativeWindow());
    m_lastFrameTime = static_cast<float>(glfwGetTime());
    while (m_running && nativeWindow != nullptr && !glfwWindowShouldClose(nativeWindow)) {
        const float time = static_cast<float>(glfwGetTime());
        m_timestep = time - m_lastFrameTime;
        m_lastFrameTime = time;

        if (m_minimized) {
            m_window->onUpdate();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }


        onUpdate(m_timestep);

        for (auto& layer : m_layerStack) {
            layer->onUpdate(m_timestep);
        }

        m_window->onUpdate();
    }

    onShutdown();
}

void Application::pushLayer(std::unique_ptr<Layer> layer)
{
    Layer* rawLayer = layer.get();
    m_layerStack.pushLayer(std::move(layer));
    rawLayer->onAttach();
}

void Application::pushOverlay(std::unique_ptr<Layer> overlay)
{
    Layer* rawOverlay = overlay.get();
    m_layerStack.pushOverlay(std::move(overlay));
    rawOverlay->onAttach();
}

void Application::handleEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
        return onWindowClose(e);
    });
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
        return onWindowResize(e);
    });

    onEventReceived(event);

    for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
        if (event.handled) {
            break;
        }

        (*it)->onEvent(event);
    }
}

bool Application::onWindowResize(const WindowResizeEvent& event)
{
    const bool minimized = event.getWidth() == 0 || event.getHeight() == 0;
    if (m_minimized != minimized) {
        m_minimized = minimized;
        onWindowMinimized(m_minimized);
    }

    if (!m_minimized) {
        onWindowResized(event.getWidth(), event.getHeight());
    }
    return false;
}

bool Application::onWindowClose(const WindowCloseEvent&)
{
    m_running = false;

    if (m_window != nullptr) {
        auto* nativeWindow = static_cast<GLFWwindow*>(m_window->getNativeWindow());
        if (nativeWindow != nullptr) {
            glfwSetWindowShouldClose(nativeWindow, GLFW_TRUE);
        }
    }

    return true;
}

} // namespace luna
