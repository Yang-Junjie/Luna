#include "Editor/editor_app.h"

#include "Editor/editor_layer.h"
#include "Core/log.h"
#include "Luna/ImGui/ImGuiLayer.hpp"
#include "Luna/Renderer/DeviceManager.hpp"
#include "Luna/Renderer/Vulkan/DeviceManager_VK.hpp"
#include "Luna/Renderer/Vulkan/VulkanRenderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cassert>
#include <string_view>

namespace luna::editor {

ApplicationSpecification EditorApp::makeSpecification()
{
    ApplicationSpecification spec;
    spec.name = "Luna Editor";
    spec.windowWidth = 1700;
    spec.windowHeight = 900;
    spec.maximized = false;
    spec.enableImGui = false;
    return spec;
}

EditorApp::EditorApp(int argc, char** argv)
    : Application(makeSpecification())
{
    parseCommandLine(argc, argv);
    if (!isInitialized()) {
        return;
    }

    if (!initializeRenderer()) {
        LUNA_CORE_ERROR("Editor renderer initialization failed");
        failInitialization();
        shutdownRenderer();
        return;
    }

    auto editorLayer = std::make_unique<EditorLayer>(*this);
    m_editorLayer = editorLayer.get();
    pushLayer(std::move(editorLayer));
}

EditorApp::~EditorApp()
{
    shutdownRenderer();
}

bool EditorApp::selfTestPassed() const
{
    return m_editorLayer != nullptr && m_editorLayer->selfTestPassed();
}

bool EditorApp::initializeImGuiForCurrentSwapchain()
{
    return initializeImGui();
}

void EditorApp::onShutdown()
{
    shutdownRenderer();
}

void EditorApp::onEventReceived(Event& event)
{
    if (m_imguiLayer != nullptr) {
        m_imguiLayer->onEvent(event);
    }
}

void EditorApp::onWindowResized(uint32_t width, uint32_t height)
{
    auto* nativeWindow = static_cast<GLFWwindow*>(getWindow().getNativeWindow());
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    if (nativeWindow != nullptr) {
        glfwGetFramebufferSize(nativeWindow, &framebufferWidth, &framebufferHeight);
    }

    LUNA_CORE_INFO("Window resized: logical={}x{}, framebuffer={}x{}",
                   width,
                   height,
                   framebufferWidth,
                   framebufferHeight);
    if (m_renderer != nullptr) {
        m_renderer->requestSwapchainRebuild();
    }
}

void EditorApp::onWindowMinimized(bool minimized)
{
    if (minimized) {
        LUNA_CORE_INFO("Window minimized; rendering paused");
    } else {
        LUNA_CORE_INFO("Window restored; rendering resumed");
        if (m_renderer != nullptr) {
            m_renderer->requestSwapchainRebuild();
        }
    }
}

void EditorApp::parseCommandLine(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--self-test=phase3_editor_resize") {
            m_selfTestMode = true;
            LUNA_CORE_INFO("Self-test mode enabled: phase3_editor_resize");
        }
    }
}

bool EditorApp::initializeRenderer()
{
    auto* nativeWindow = static_cast<GLFWwindow*>(getWindow().getNativeWindow());
    if (nativeWindow == nullptr) {
        return false;
    }

    m_deviceManager = renderer::DeviceManager::create(renderer::GraphicsAPI::Vulkan);
    if (m_deviceManager == nullptr) {
        return false;
    }

    assert(m_deviceManager->graphicsAPI() == renderer::GraphicsAPI::Vulkan);
    m_vulkanDeviceManager = static_cast<renderer::vulkan::DeviceManager_VK*>(m_deviceManager.get());
    m_renderer = std::make_unique<renderer::vulkan::VulkanRenderer>();

#ifndef NDEBUG
    constexpr bool kEnableValidation = true;
#else
    constexpr bool kEnableValidation = false;
#endif

    renderer::DeviceManagerCreateInfo createInfo{};
    createInfo.appName = "Luna Editor";
    createInfo.engineName = "Luna";
    createInfo.apiVersion = VK_API_VERSION_1_1;
    createInfo.enableValidation = kEnableValidation;

    if (!m_vulkanDeviceManager->initialize(nativeWindow, createInfo) ||
        !m_renderer->initialize(*m_vulkanDeviceManager, nativeWindow) || !initializeImGui()) {
        return false;
    }

    m_rendererInitialized = true;
    LUNA_CORE_INFO("Renderer initialized via Vulkan backend");
    return true;
}

bool EditorApp::initializeImGui()
{
    auto* nativeWindow = static_cast<GLFWwindow*>(getWindow().getNativeWindow());
    if (nativeWindow == nullptr || m_renderer == nullptr || m_vulkanDeviceManager == nullptr) {
        return false;
    }

    if (m_imguiLayer == nullptr) {
        m_imguiLayer = std::make_unique<ImGuiLayer>();
    }

    ImGuiVulkanBackendConfig config{};
    config.window = nativeWindow;
    config.apiVersion = m_vulkanDeviceManager->apiVersion();
    config.instance = m_vulkanDeviceManager->instance();
    config.physicalDevice = m_vulkanDeviceManager->physicalDevice();
    config.device = m_vulkanDeviceManager->device();
    config.queueFamily = m_vulkanDeviceManager->graphicsQueueFamily();
    config.queue = m_vulkanDeviceManager->graphicsQueue();
    config.renderPass = m_renderer->renderPass();
    config.minImageCount = m_renderer->minImageCount();
    config.imageCount = m_renderer->imageCount();
    return m_imguiLayer->initialize(config);
}

void EditorApp::shutdownRenderer()
{
    if (m_vulkanDeviceManager != nullptr && m_vulkanDeviceManager->device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_vulkanDeviceManager->device());
    }

    if (m_imguiLayer != nullptr) {
        m_imguiLayer->shutdown();
        m_imguiLayer.reset();
    }

    if (m_renderer != nullptr) {
        m_renderer->shutdown();
        m_renderer.reset();
    }

    if (m_deviceManager != nullptr) {
        m_deviceManager->shutdown();
        m_deviceManager.reset();
    }
    m_vulkanDeviceManager = nullptr;

    if (!m_rendererInitialized) {
        return;
    }

    m_rendererInitialized = false;
    LUNA_CORE_INFO("Renderer shutdown complete");
}

} // namespace luna::editor

namespace luna {

Application* createApplication(int argc, char** argv)
{
    return new editor::EditorApp(argc, argv);
}

} // namespace luna
