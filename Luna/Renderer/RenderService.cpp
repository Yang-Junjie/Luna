#include "Renderer/RenderService.h"

#include "Core/log.h"

#include <cstdlib>

namespace luna {

bool RenderService::init(Window& window, const RenderServiceSpecification& specification)
{
    m_backend = specification.backend;
    if (specification.backend != RHIBackend::Vulkan) {
        LUNA_CORE_ERROR("Unsupported backend: {}", to_string(specification.backend));
        return false;
    }

    vk::ClearColorValue clearColor{};
    clearColor.float32[0] = specification.demoClearColor[0];
    clearColor.float32[1] = specification.demoClearColor[1];
    clearColor.float32[2] = specification.demoClearColor[2];
    clearColor.float32[3] = specification.demoClearColor[3];

    m_vulkanEngine.setDemoMode(specification.demoMode);
    m_vulkanEngine.setDemoClearColor(clearColor);
    m_vulkanEngine.setTriangleShaderPaths(
        specification.triangleVertexShaderPath, specification.triangleFragmentShaderPath);

    m_initialized = m_vulkanEngine.init(window);
    if (!m_initialized) {
        return false;
    }

    if (specification.demoMode == VulkanEngine::DemoMode::LegacyScene) {
        LUNA_CORE_INFO("Renderer initialized via RHI backend: {}", to_string(m_backend));
        LUNA_CORE_INFO("Scene path=RHI");
    }
    LUNA_CORE_INFO("RenderService initialized");
    LUNA_CORE_INFO("Application render path=RHI");
    return true;
}

void RenderService::shutdown()
{
    if (!m_initialized) {
        return;
    }

    m_vulkanEngine.cleanup();
    m_initialized = false;
}

void RenderService::draw(const VulkanEngine::OverlayRenderFunction& overlayRenderer,
                         const VulkanEngine::BeforePresentFunction& beforePresent)
{
    m_vulkanEngine.draw(overlayRenderer, beforePresent);
}

void RenderService::request_swapchain_resize()
{
    m_vulkanEngine.request_swapchain_resize();
}

bool RenderService::is_swapchain_resize_requested() const
{
    return m_vulkanEngine.is_swapchain_resize_requested();
}

bool RenderService::resize_swapchain()
{
    return m_vulkanEngine.resize_swapchain();
}

vk::Format RenderService::getSwapchainImageFormat() const
{
    return m_vulkanEngine.getSwapchainImageFormat();
}

uint32_t RenderService::getSwapchainImageCount() const
{
    return m_vulkanEngine.getSwapchainImageCount();
}

bool RenderService::uploadTriangleVertices(std::span<const TriangleVertex> vertices)
{
    return m_vulkanEngine.uploadTriangleVertices(vertices);
}

float& RenderService::getRenderScale()
{
    return m_vulkanEngine.renderScale;
}

Camera& RenderService::getMainCamera()
{
    return m_vulkanEngine.mainCamera;
}

std::vector<ComputeEffect>& RenderService::getBackgroundEffects()
{
    return m_vulkanEngine.get_background_effects();
}

int& RenderService::getCurrentBackgroundEffect()
{
    return m_vulkanEngine.get_current_background_effect();
}

std::shared_ptr<LoadedGLTF> RenderService::findLoadedScene(std::string_view sceneName) const
{
    const auto sceneIt = m_vulkanEngine.loadedScenes.find(std::string(sceneName));
    if (sceneIt == m_vulkanEngine.loadedScenes.end()) {
        return {};
    }

    return sceneIt->second;
}

NativeVulkanBridge RenderService::getNativeVulkanBridge()
{
    if (!m_loggedNativeBridge) {
        LUNA_CORE_INFO("Native Vulkan bridge enabled for legacy modules");
        m_loggedNativeBridge = true;
    }

    return NativeVulkanBridge{.engine = &m_vulkanEngine};
}

VulkanEngine& RenderService::requireNativeVulkanEngine()
{
    NativeVulkanBridge bridge = getNativeVulkanBridge();
    if (bridge.engine == nullptr) {
        LUNA_CORE_FATAL("Native Vulkan bridge is unavailable");
        std::abort();
    }

    return *bridge.engine;
}

} // namespace luna
