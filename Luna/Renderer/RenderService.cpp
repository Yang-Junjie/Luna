#include "Renderer/RenderService.h"

#include "Core/log.h"
#include "Imgui/ImGuiLayer.hpp"
#include "Renderer/RenderPipeline.h"
#include "Vulkan/vk_rhi_device.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>

namespace luna {
namespace {

VulkanEngine::LegacyRendererMode to_legacy_renderer_mode(LegacyRendererKind legacyRenderer)
{
    switch (legacyRenderer) {
        case LegacyRendererKind::ClearColor:
            return VulkanEngine::LegacyRendererMode::ClearColor;
        case LegacyRendererKind::Triangle:
            return VulkanEngine::LegacyRendererMode::Triangle;
        case LegacyRendererKind::ComputeBackground:
            return VulkanEngine::LegacyRendererMode::ComputeBackground;
        case LegacyRendererKind::LegacyScene:
        default:
            return VulkanEngine::LegacyRendererMode::LegacyScene;
    }
}

} // namespace

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
    auto& legacyEffects = m_vulkanEngine.get_background_effects();
    m_legacyBackgroundEffects.clear();
    m_legacyBackgroundEffects.reserve(legacyEffects.size());
    for (const auto& legacyEffect : legacyEffects) {
        SceneBackgroundEffect effect{};
        effect.name = legacyEffect.name != nullptr ? legacyEffect.name : "";
        effect.data.data1 = legacyEffect.data.data1;
        effect.data.data2 = legacyEffect.data.data2;
        effect.data.data3 = legacyEffect.data.data3;
        effect.data.data4 = legacyEffect.data.data4;
        m_legacyBackgroundEffects.push_back(std::move(effect));
    }
}

void RenderService::syncLegacyBackgroundEffectsToEngine()
{
    auto& legacyEffects = m_vulkanEngine.get_background_effects();
    const size_t effectCount = std::min(m_legacyBackgroundEffects.size(), legacyEffects.size());
    for (size_t index = 0; index < effectCount; ++index) {
        legacyEffects[index].data.data1 = m_legacyBackgroundEffects[index].data.data1;
        legacyEffects[index].data.data2 = m_legacyBackgroundEffects[index].data.data2;
        legacyEffects[index].data.data3 = m_legacyBackgroundEffects[index].data.data3;
        legacyEffects[index].data.data4 = m_legacyBackgroundEffects[index].data.data4;
    }
}

bool RenderService::init(Window& window, const RenderServiceSpecification& specification)
{
    m_backend = specification.backend;
    m_renderPipeline = specification.renderPipeline;

    if (m_renderPipeline != nullptr) {
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
        createInfo.swapchain =
            {.width = window.getWidth(), .height = window.getHeight(), .bufferCount = 2, .format = PixelFormat::BGRA8Unorm};

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

    if (specification.backend != RHIBackend::Vulkan) {
        LUNA_CORE_ERROR("Unsupported backend: {}", to_string(specification.backend));
        return false;
    }

    m_vulkanEngine.setLegacyRendererMode(to_legacy_renderer_mode(specification.legacyRenderer));
    m_vulkanEngine.setDemoClearColor(specification.demoClearColor[0],
                                     specification.demoClearColor[1],
                                     specification.demoClearColor[2],
                                     specification.demoClearColor[3]);
    m_vulkanEngine.setTriangleShaderPaths(
        specification.triangleVertexShaderPath, specification.triangleFragmentShaderPath);

    m_initialized = m_vulkanEngine.init(window);
    if (!m_initialized) {
        return false;
    }

    if (specification.legacyRenderer == LegacyRendererKind::LegacyScene) {
        LUNA_CORE_INFO("Renderer initialized via RHI backend: {}", to_string(m_backend));
        LUNA_CORE_INFO("Scene path=RHI");
    }
    LUNA_CORE_INFO("这是旧路径基线");
    LUNA_CORE_INFO("RenderService initialized");
    LUNA_CORE_INFO("Application render path=RHI");
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
        return;
    }

    m_vulkanEngine.cleanup();
    m_initialized = false;
}

void RenderService::draw(ImGuiLayer* imguiLayer)
{
    if (m_rhiDevice != nullptr) {
        auto* vulkanDevice = dynamic_cast<VulkanRHIDevice*>(m_rhiDevice.get());

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
            if (vulkanDevice != nullptr) {
                const RHIResult overlayResult = vulkanDevice->renderOverlay(*imguiLayer);
                if (overlayResult != RHIResult::Success) {
                    LUNA_CORE_ERROR("ImGui overlay render failed: {}", to_string(overlayResult));
                }
            } else if (!m_loggedUnsupportedImGui) {
                LUNA_CORE_WARN("ImGui overlay is only supported on the Vulkan RHI path");
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
        return;
    }

    if (imguiLayer != nullptr) {
        syncLegacyBackgroundEffectsToEngine();
        m_vulkanEngine.drawImGui(imguiLayer);
        return;
    }

    syncLegacyBackgroundEffectsToEngine();
    m_vulkanEngine.draw();
}

std::unique_ptr<ImGuiLayer> RenderService::createImGuiLayer(void* nativeWindow, bool enableMultiViewport)
{
    if (!m_initialized || nativeWindow == nullptr) {
        return {};
    }

    if (m_rhiDevice != nullptr) {
        auto* vulkanDevice = dynamic_cast<VulkanRHIDevice*>(m_rhiDevice.get());
        if (vulkanDevice == nullptr) {
            LUNA_CORE_ERROR("ImGui overlay factory requires the Vulkan RHI backend");
            return {};
        }

        return std::make_unique<ImGuiLayer>(
            static_cast<GLFWwindow*>(nativeWindow), vulkanDevice->getEngine(), enableMultiViewport);
    }

    return std::make_unique<ImGuiLayer>(static_cast<GLFWwindow*>(nativeWindow), m_vulkanEngine, enableMultiViewport);
}

void RenderService::request_swapchain_resize()
{
    if (m_rhiDevice != nullptr) {
        return;
    }

    m_vulkanEngine.request_swapchain_resize();
}

bool RenderService::is_swapchain_resize_requested() const
{
    if (m_rhiDevice != nullptr) {
        return false;
    }

    return m_vulkanEngine.is_swapchain_resize_requested();
}

bool RenderService::resize_swapchain()
{
    if (m_rhiDevice != nullptr) {
        return true;
    }

    return m_vulkanEngine.resize_swapchain();
}

uint32_t RenderService::getSwapchainImageCount() const
{
    if (m_rhiDevice != nullptr) {
        return m_rhiDevice->getCapabilities().framesInFlight;
    }

    return m_vulkanEngine.getSwapchainImageCount();
}

bool RenderService::uploadTriangleVertices(std::span<const TriangleVertex> vertices)
{
    if (m_rhiDevice != nullptr) {
        LUNA_CORE_ERROR("uploadTriangleVertices is only available on the legacy Vulkan path");
        return false;
    }

    return m_vulkanEngine.uploadTriangleVertices(vertices);
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

    return m_vulkanEngine.renderScale;
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

    return m_vulkanEngine.mainCamera;
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

    syncLegacyBackgroundEffectsFromEngine();
    return m_legacyBackgroundEffects;
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

    return m_vulkanEngine.get_current_background_effect();
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
