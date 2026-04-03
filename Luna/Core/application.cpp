#include "application.h"

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
    m_window = Window::create(WindowProps{
        m_specification.name, m_specification.windowWidth, m_specification.windowHeight, m_specification.maximized});

    if (m_window == nullptr) {
        LUNA_CORE_ERROR("Application window creation failed");
        s_instance = nullptr;
        return;
    }

    m_window->setEventCallback([this](Event& event) {
        onEvent(event);
    });

    if (!m_engine.init(*m_window)) {
        LUNA_CORE_ERROR("Renderer initialization failed");
        m_window.reset();
        s_instance = nullptr;
        return;
    }

    auto* nativeWindow = static_cast<GLFWwindow*>(m_window->getNativeWindow());
    m_imGuiLayer = std::make_unique<ImGuiLayer>(nativeWindow, m_engine, m_specification.enableMultiViewport);
    m_imGuiLayerRaw = m_imGuiLayer.get();
    m_imGuiLayer->onAttach();

    if (!m_imGuiLayer->isInitialized()) {
        LUNA_CORE_ERROR("ImGui initialization failed");
        m_imGuiLayer.reset();
        m_imGuiLayerRaw = nullptr;
        m_engine.cleanup();
        m_window.reset();
        s_instance = nullptr;
        return;
    }

    m_initialized = true;
}

Application::~Application()
{
    if (m_imGuiLayer != nullptr) {
        m_imGuiLayer->onDetach();
        m_imGuiLayer.reset();
        m_imGuiLayerRaw = nullptr;
    }

    m_engine.cleanup();
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

        if (m_engine.is_swapchain_resize_requested()) {
            m_engine.resize_swapchain();
        }

        onUpdate(m_timestep);

        for (auto& layer : m_layerStack) {
            layer->onUpdate(m_timestep);
        }

        renderFrame();
        m_window->onUpdate();
    }

    onShutdown();
}

void Application::renderFrame()
{
    if (m_imGuiLayer != nullptr && m_imGuiLayer->isInitialized()) {
        m_imGuiLayer->begin();

        for (auto& layer : m_layerStack) {
            layer->onImGuiRender();
        }

        m_imGuiLayer->end();
    }

    m_engine.draw(
        [this](vk::CommandBuffer commandBuffer, vk::ImageView targetImageView, vk::Extent2D targetExtent) {
            if (m_imGuiLayer != nullptr) {
                m_imGuiLayer->render(commandBuffer, targetImageView, targetExtent);
            }
        },
        [this]() {
            if (m_imGuiLayer != nullptr) {
                m_imGuiLayer->renderPlatformWindows();
            }
        });
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

void Application::onEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
        return onWindowClose(e);
    });
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
        return onWindowResize(e);
    });

    if (m_imGuiLayer != nullptr) {
        m_imGuiLayer->onEvent(event);
    }

    for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
        if (event.handled) {
            break;
        }

        (*it)->onEvent(event);
    }
}

bool Application::onWindowResize(const WindowResizeEvent& event)
{
    m_minimized = event.getWidth() == 0 || event.getHeight() == 0;
    if (!m_minimized) {
        m_engine.request_swapchain_resize();
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
