#include "Renderer/RenderService.h"

#include "Core/log.h"
#include "Imgui/ImGuiLayer.hpp"
#include "Renderer/RenderPipeline.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>

namespace luna {

ISceneController* RenderService::getSceneController()
{
    return dynamic_cast<ISceneController*>(m_renderPipeline.get());
}

const ISceneController* RenderService::getSceneController() const
{
    return dynamic_cast<const ISceneController*>(m_renderPipeline.get());
}

void RenderService::syncLegacyBackgroundEffectsFromEngine()
{
}

void RenderService::syncLegacyBackgroundEffectsToEngine()
{
}

bool RenderService::init(Window& window, const RenderServiceSpecification& specification)
{
    m_backend = specification.backend;
    m_renderPipeline = specification.renderPipeline;

    if (m_renderPipeline == nullptr) {
        LUNA_CORE_ERROR("RenderService requires an explicit renderPipeline; legacy Vulkan fallback has been removed");
        return false;
    }

    m_rhiDevice = CreateRHIDevice(specification.backend);
    if (!m_rhiDevice) {
        LUNA_CORE_ERROR("Failed to create RHI device for {}", to_string(specification.backend));
        m_renderPipeline.reset();
        return false;
    }

    DeviceCreateInfo createInfo{};
    createInfo.applicationName = specification.applicationName;
    createInfo.backend = specification.backend;
    createInfo.nativeWindow = window.getNativeWindow();
    createInfo.swapchain = specification.swapchain;
    if (createInfo.swapchain.width == 0) {
        createInfo.swapchain.width = window.getWidth();
    }
    if (createInfo.swapchain.height == 0) {
        createInfo.swapchain.height = window.getHeight();
    }
    if (createInfo.swapchain.format == PixelFormat::Undefined) {
        createInfo.swapchain.format = PixelFormat::BGRA8Unorm;
    }

    const RHIResult initResult = m_rhiDevice->init(createInfo);
    if (initResult != RHIResult::Success) {
        LUNA_CORE_ERROR("RHI device init failed: {}", to_string(initResult));
        m_rhiDevice.reset();
        m_renderPipeline.reset();
        return false;
    }

    if (!m_renderPipeline->init(*m_rhiDevice)) {
        LUNA_CORE_ERROR("IRenderPipeline initialization failed");
        m_rhiDevice->shutdown();
        m_rhiDevice.reset();
        m_renderPipeline.reset();
        return false;
    }

    m_initialized = true;
    LUNA_CORE_INFO("RenderService initialized");
    LUNA_CORE_INFO("Application render path=RHI pipeline");
    return true;
}

void RenderService::shutdown()
{
    if (!m_initialized) {
        return;
    }

    if (m_rhiDevice != nullptr) {
        if (m_renderPipeline != nullptr) {
            if (m_rhiDevice->waitIdle() != RHIResult::Success) {
                LUNA_CORE_WARN("RHI waitIdle failed during RenderService shutdown");
            }
            m_renderPipeline->shutdown(*m_rhiDevice);
        }
        m_rhiDevice->shutdown();
        m_rhiDevice.reset();
        m_renderPipeline.reset();
        m_initialized = false;
    }
    m_initialized = false;
}

void RenderService::draw(ImGuiLayer* imguiLayer)
{
    FrameContext frameContext{};
    const RHIResult beginFrameResult = m_rhiDevice->beginFrame(&frameContext);
    if (beginFrameResult == RHIResult::NotReady) {
        return;
    }
    if (beginFrameResult != RHIResult::Success) {
        LUNA_CORE_ERROR("RHI beginFrame failed: {}", to_string(beginFrameResult));
        return;
    }

    bool renderSucceeded = m_renderPipeline != nullptr && m_renderPipeline->render(*m_rhiDevice, frameContext);
    if (!renderSucceeded) {
        LUNA_CORE_ERROR("IRenderPipeline render failed");
    }

    if (imguiLayer != nullptr) {
        if (imguiLayer->render(*m_rhiDevice, frameContext)) {
            m_loggedUnsupportedImGui = false;
        } else if (!m_loggedUnsupportedImGui) {
            LUNA_CORE_WARN("ImGui overlay is not available on the active public RHI render path");
            m_loggedUnsupportedImGui = true;
        }
    }

    const RHIResult endFrameResult = m_rhiDevice->endFrame();
    if (endFrameResult != RHIResult::Success) {
        LUNA_CORE_ERROR("RHI endFrame failed: {}", to_string(endFrameResult));
        return;
    }

    const RHIResult presentResult = m_rhiDevice->present();
    if (presentResult != RHIResult::Success) {
        LUNA_CORE_ERROR("RHI present failed: {}", to_string(presentResult));
    } else if (imguiLayer != nullptr) {
        imguiLayer->renderPlatformWindows();
    }
}

std::unique_ptr<ImGuiLayer> RenderService::createImGuiLayer(void* nativeWindow, bool enableMultiViewport)
{
    if (!m_initialized || nativeWindow == nullptr) {
        return {};
    }

    if (m_rhiDevice != nullptr) {
        return std::make_unique<ImGuiLayer>(static_cast<GLFWwindow*>(nativeWindow), *m_rhiDevice, enableMultiViewport);
    }

    return {};
}

void RenderService::request_swapchain_resize()
{
    (void) m_rhiDevice;
}

bool RenderService::is_swapchain_resize_requested() const
{
    return false;
}

bool RenderService::resize_swapchain()
{
    return true;
}

uint32_t RenderService::getSwapchainImageCount() const
{
    return m_rhiDevice != nullptr ? m_rhiDevice->getSwapchainState().imageCount : 0;
}

float& RenderService::getRenderScale()
{
    if (m_rhiDevice != nullptr) {
        if (ISceneController* controller = getSceneController()) {
            return controller->renderScale();
        }
        static float s_renderScale = 1.0f;
        return s_renderScale;
    }

    static float s_renderScale = 1.0f;
    return s_renderScale;
}

Camera& RenderService::getMainCamera()
{
    if (m_rhiDevice != nullptr) {
        if (ISceneController* controller = getSceneController()) {
            return controller->camera();
        }
        static Camera s_camera{};
        return s_camera;
    }

    static Camera s_camera{};
    return s_camera;
}

std::vector<SceneBackgroundEffect>& RenderService::getBackgroundEffects()
{
    if (m_rhiDevice != nullptr) {
        if (ISceneController* controller = getSceneController()) {
            return controller->backgroundEffects();
        }
        static std::vector<SceneBackgroundEffect> s_effects;
        return s_effects;
    }

    static std::vector<SceneBackgroundEffect> s_effects;
    return s_effects;
}

int& RenderService::getCurrentBackgroundEffect()
{
    if (m_rhiDevice != nullptr) {
        if (ISceneController* controller = getSceneController()) {
            return controller->currentBackgroundEffect();
        }
        static int s_effectIndex = 0;
        return s_effectIndex;
    }

    static int s_effectIndex = 0;
    return s_effectIndex;
}

std::shared_ptr<SceneDocument> RenderService::findLoadedScene(std::string_view sceneName) const
{
    if (m_rhiDevice != nullptr) {
        if (const ISceneController* controller = getSceneController()) {
            return controller->findScene(sceneName);
        }
        return {};
    }

    return {};
}

} // namespace luna
