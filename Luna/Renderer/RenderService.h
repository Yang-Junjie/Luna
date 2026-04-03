#pragma once

#include "Core/window.h"
#include "RHI/Types.h"
#include "Vulkan/vk_engine.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace luna {

struct RenderServiceSpecification {
    std::string_view applicationName = "Luna";
    RHIBackend backend = RHIBackend::Vulkan;
    VulkanEngine::DemoMode demoMode = VulkanEngine::DemoMode::LegacyScene;
    std::array<float, 4> demoClearColor{0.08f, 0.12f, 0.18f, 1.0f};
    std::string triangleVertexShaderPath;
    std::string triangleFragmentShaderPath;
};

struct NativeVulkanBridge {
    VulkanEngine* engine = nullptr;
};

class RenderService {
public:
    bool init(Window& window, const RenderServiceSpecification& specification);
    void shutdown();

    void draw(const VulkanEngine::OverlayRenderFunction& overlayRenderer = {},
              const VulkanEngine::BeforePresentFunction& beforePresent = {});

    void request_swapchain_resize();
    bool is_swapchain_resize_requested() const;
    bool resize_swapchain();

    vk::Format getSwapchainImageFormat() const;
    uint32_t getSwapchainImageCount() const;
    bool uploadTriangleVertices(std::span<const TriangleVertex> vertices);
    float& getRenderScale();
    Camera& getMainCamera();
    std::vector<ComputeEffect>& getBackgroundEffects();
    int& getCurrentBackgroundEffect();
    std::shared_ptr<LoadedGLTF> findLoadedScene(std::string_view sceneName) const;

    NativeVulkanBridge getNativeVulkanBridge();
    VulkanEngine& requireNativeVulkanEngine();
    RHIBackend getBackend() const
    {
        return m_backend;
    }

private:
    RHIBackend m_backend = RHIBackend::Vulkan;
    bool m_initialized = false;
    bool m_loggedNativeBridge = false;
    VulkanEngine m_vulkanEngine;
};

} // namespace luna
