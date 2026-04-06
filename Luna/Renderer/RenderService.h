#pragma once

#include "Core/window.h"
#include "RHI/RHIDevice.h"
#include "RHI/Types.h"
#include "Renderer/SceneRenderPipeline.h"

#include <memory>
#include <string_view>

namespace luna {

class ImGuiLayer;
class IRenderPipeline;

struct RenderServiceSpecification {
    std::string_view applicationName = "Luna";
    RHIBackend backend = RHIBackend::Vulkan;
    SwapchainDesc swapchain{};
    std::shared_ptr<IRenderPipeline> renderPipeline;
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
    float& getRenderScale();
    Camera& getMainCamera();
    std::vector<SceneBackgroundEffect>& getBackgroundEffects();
    int& getCurrentBackgroundEffect();
    std::shared_ptr<SceneDocument> findLoadedScene(std::string_view sceneName) const;
    IRHIDevice* getRHIDevice()
    {
        return m_rhiDevice.get();
    }

    const IRHIDevice* getRHIDevice() const
    {
        return m_rhiDevice.get();
    }

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
};

} // namespace luna
