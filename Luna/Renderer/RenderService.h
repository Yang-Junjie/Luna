#pragma once

#include "Core/window.h"
#include "RHI/RHIDevice.h"
#include "RHI/Types.h"
#include "Renderer/SceneRenderPipeline.h"
#include "Vulkan/vk_engine.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace luna {

class ImGuiLayer;
class IRenderPipeline;

enum class LegacyRendererKind : uint8_t {
    LegacyScene = 0,
    ClearColor,
    Triangle,
    ComputeBackground
};

struct RenderServiceSpecification {
    std::string_view applicationName = "Luna";
    RHIBackend backend = RHIBackend::Vulkan;
    std::shared_ptr<IRenderPipeline> renderPipeline;
    LegacyRendererKind legacyRenderer = LegacyRendererKind::LegacyScene;
    std::array<float, 4> demoClearColor{0.08f, 0.12f, 0.18f, 1.0f};
    std::string triangleVertexShaderPath;
    std::string triangleFragmentShaderPath;
};

class RenderService {
public:
    bool init(Window& window, const RenderServiceSpecification& specification);
    void shutdown();

    void draw(ImGuiLayer* imguiLayer = nullptr);
    std::unique_ptr<ImGuiLayer> createImGuiLayer(void* nativeWindow, bool enableMultiViewport);

    void request_swapchain_resize();
    bool is_swapchain_resize_requested() const;
    bool resize_swapchain();

    uint32_t getSwapchainImageCount() const;
    bool uploadTriangleVertices(std::span<const TriangleVertex> vertices);
    float& getRenderScale();
    Camera& getMainCamera();
    std::vector<SceneBackgroundEffect>& getBackgroundEffects();
    int& getCurrentBackgroundEffect();
    std::shared_ptr<SceneDocument> findLoadedScene(std::string_view sceneName) const;
    RHIBackend getBackend() const
    {
        return m_backend;
    }

private:
    ISceneController* getSceneController();
    const ISceneController* getSceneController() const;
    void syncLegacyBackgroundEffectsFromEngine();
    void syncLegacyBackgroundEffectsToEngine();

private:
    RHIBackend m_backend = RHIBackend::Vulkan;
    bool m_initialized = false;
    bool m_loggedUnsupportedImGui = false;
    std::shared_ptr<IRenderPipeline> m_renderPipeline;
    std::unique_ptr<IRHIDevice> m_rhiDevice;
    std::vector<SceneBackgroundEffect> m_legacyBackgroundEffects;
    VulkanEngine m_vulkanEngine;
};

} // namespace luna
