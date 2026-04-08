#include "Application.h"

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
    m_window = Window::create(WindowProps{
        m_specification.m_name, m_specification.m_window_width, m_specification.m_window_height, m_specification.m_maximized});

    if (m_window == nullptr) {
        LUNA_CORE_ERROR("Application window creation failed");
        m_s_instance = nullptr;
        return;
    }

    m_window->setEventCallback([this](Event& event) {
        onEvent(event);
    });

    if (!m_engine.init(*m_window)) {
        LUNA_CORE_ERROR("Renderer initialization failed");
        m_window.reset();
        m_s_instance = nullptr;
        return;
    }

    auto* native_window = static_cast<GLFWwindow*>(m_window->getNativeWindow());
    m_im_gui_layer = std::make_unique<ImGuiLayer>(native_window, m_engine, m_specification.m_enable_multi_viewport);
    m_im_gui_layer_raw = m_im_gui_layer.get();
    m_im_gui_layer->onAttach();

    if (!m_im_gui_layer->isInitialized()) {
        LUNA_CORE_ERROR("ImGui initialization failed");
        m_im_gui_layer.reset();
        m_im_gui_layer_raw = nullptr;
        m_engine.cleanup();
        m_window.reset();
        m_s_instance = nullptr;
        return;
    }

    m_initialized = true;
}

Application::~Application()
{
    if (m_im_gui_layer != nullptr) {
        m_im_gui_layer->onDetach();
        m_im_gui_layer.reset();
        m_im_gui_layer_raw = nullptr;
    }

    m_engine.cleanup();
    m_s_instance = nullptr;
}

void Application::run()
{
    if (!m_initialized) {
        LUNA_CORE_ERROR("Cannot run application because initialization failed");
        return;
    }

    onInit();

    auto* native_window = static_cast<GLFWwindow*>(m_window->getNativeWindow());
    m_last_frame_time = static_cast<float>(glfwGetTime());
    while (m_running && native_window != nullptr && !glfwWindowShouldClose(native_window)) {
        const float time = static_cast<float>(glfwGetTime());
        m_timestep = time - m_last_frame_time;
        m_last_frame_time = time;

        if (m_minimized) {
            m_window->onUpdate();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        if (m_engine.isSwapchainResizeRequested()) {
            m_engine.resizeSwapchain();
        }

        onUpdate(m_timestep);

        for (auto& layer : m_layer_stack) {
            layer->onUpdate(m_timestep);
        }

        renderFrame();
        m_window->onUpdate();
    }

    onShutdown();
}

void Application::renderFrame()
{
    if (m_im_gui_layer != nullptr && m_im_gui_layer->isInitialized()) {
        m_im_gui_layer->begin();

        for (auto& layer : m_layer_stack) {
            layer->onImGuiRender();
        }

        m_im_gui_layer->end();
    }

    m_engine.draw(
        [this](vk::CommandBuffer command_buffer, vk::ImageView target_image_view, vk::Extent2D target_extent) {
            if (m_im_gui_layer != nullptr) {
                m_im_gui_layer->render(command_buffer, target_image_view, target_extent);
            }
        },
        [this]() {
            if (m_im_gui_layer != nullptr) {
                m_im_gui_layer->renderPlatformWindows();
            }
        });
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

    if (m_im_gui_layer != nullptr) {
        m_im_gui_layer->onEvent(event);
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
        m_engine.requestSwapchainResize();
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

