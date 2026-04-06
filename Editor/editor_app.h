#pragma once

#include "Core/application.h"

#include <array>
#include <memory>

namespace luna {
class ImGuiLayer;
}

namespace luna::renderer::vulkan {
class DeviceManager_VK;
class VulkanRenderer;
}

namespace luna::renderer {
class DeviceManager;
}

namespace luna::editor {

class EditorLayer;

class EditorApp final : public Application {
public:
    EditorApp(int argc, char** argv);
    ~EditorApp() override;

    bool isSelfTestMode() const { return m_selfTestMode; }
    bool selfTestPassed() const;

    renderer::vulkan::DeviceManager_VK* vulkanDeviceManager() const { return m_vulkanDeviceManager; }
    renderer::vulkan::VulkanRenderer* renderer() const { return m_renderer.get(); }
    ImGuiLayer* imguiLayer() const { return m_imguiLayer.get(); }
    bool initializeImGuiForCurrentSwapchain();

protected:
    void onShutdown() override;
    void onEventReceived(Event& event) override;
    void onWindowResized(uint32_t width, uint32_t height) override;
    void onWindowMinimized(bool minimized) override;

private:
    static ApplicationSpecification makeSpecification();

    void parseCommandLine(int argc, char** argv);
    bool initializeRenderer();
    bool initializeImGui();
    void shutdownRenderer();

    std::unique_ptr<renderer::DeviceManager> m_deviceManager;
    renderer::vulkan::DeviceManager_VK* m_vulkanDeviceManager = nullptr;
    std::unique_ptr<renderer::vulkan::VulkanRenderer> m_renderer;
    std::unique_ptr<ImGuiLayer> m_imguiLayer;
    EditorLayer* m_editorLayer = nullptr;
    bool m_rendererInitialized = false;
    bool m_selfTestMode = false;
};

} // namespace luna::editor
