#include "Editor/editor_layer.h"

#include "Core/application.h"
#include "Core/log.h"
#include "Editor/editor_app.h"
#include "Luna/ImGui/ImGuiLayer.hpp"
#include "Luna/Renderer/Vulkan/DeviceManager_VK.hpp"
#include "Luna/Renderer/Vulkan/VulkanRenderer.hpp"

#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>

namespace luna::editor {

EditorLayer::EditorLayer(EditorApp& app)
    : Layer("EditorLayer"),
      m_app(app)
{}

void EditorLayer::onUpdate(Timestep timestep)
{
    m_elapsedSeconds += timestep.getSeconds();
    updateSelfTest();

    auto* nativeWindow = static_cast<GLFWwindow*>(Application::get().getWindow().getNativeWindow());
    auto* renderer = m_app.renderer();
    auto* imguiLayer = m_app.imguiLayer();
    if (nativeWindow == nullptr || renderer == nullptr || imguiLayer == nullptr) {
        m_app.close();
        return;
    }

    const bool needSwapchainRebuild = renderer->isSwapchainDirty();
    switch (renderer->recreateSwapchain(nativeWindow)) {
        case renderer::vulkan::SwapchainRebuildResult::Ready:
            if (needSwapchainRebuild) {
                imguiLayer->setMinImageCount(renderer->minImageCount());
            }
            break;
        case renderer::vulkan::SwapchainRebuildResult::RenderPassChanged:
            if (!m_app.initializeImGuiForCurrentSwapchain()) {
                m_app.close();
                return;
            }
            break;
        case renderer::vulkan::SwapchainRebuildResult::Deferred:
            return;
        case renderer::vulkan::SwapchainRebuildResult::Failed:
        default:
            m_app.close();
            return;
    }

    imguiLayer->beginFrame();
    buildUi(timestep.getMilliseconds());
    imguiLayer->endFrame();

    if (!renderer->renderFrame(ImGui::GetDrawData(), computeClearColor())) {
        m_app.close();
    }
}

void EditorLayer::updateSelfTest()
{
    if (!m_app.isSelfTestMode()) {
        return;
    }

    auto* nativeWindow = static_cast<GLFWwindow*>(Application::get().getWindow().getNativeWindow());
    if (nativeWindow == nullptr) {
        return;
    }

    if (m_selfTestPhase == 0 && m_elapsedSeconds >= 0.6f) {
        glfwSetWindowSize(nativeWindow, 1280, 720);
        LUNA_CORE_INFO("Self-test step: resized window to 1280x720");
        ++m_selfTestPhase;
    } else if (m_selfTestPhase == 1 && m_elapsedSeconds >= 1.2f) {
        glfwSetWindowSize(nativeWindow, 1600, 900);
        LUNA_CORE_INFO("Self-test step: resized window to 1600x900");
        ++m_selfTestPhase;
    } else if (m_selfTestPhase == 2 && m_elapsedSeconds >= 1.8f) {
        Application::get().getWindow().setMinimized();
        Application::get().getWindow().setRestored();
        LUNA_CORE_INFO("Self-test step: minimized and restored window");
        ++m_selfTestPhase;
    } else if (m_selfTestPhase == 3 && m_elapsedSeconds >= 2.8f) {
        m_selfTestPassed = true;
        LUNA_CORE_INFO("Self-test completed successfully");
        m_app.close();
        ++m_selfTestPhase;
    }
}

void EditorLayer::buildUi(float frameTimeMs)
{
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ImGui Ready")) {
        ImGui::TextUnformatted("GLFW + Vulkan path is active.");
        ImGui::Text("GPU: %s",
                    m_app.vulkanDeviceManager() != nullptr ? m_app.vulkanDeviceManager()->gpuName().c_str()
                                                           : "Unavailable");
        ImGui::Text("Frame time: %.2f ms", frameTimeMs);
        ImGui::Checkbox("Warm Background", &m_warmBackground);
        ImGui::SliderFloat("Background Mix", &m_backgroundMix, 0.0f, 1.0f);
    }
    ImGui::End();

    ImGui::ShowDemoWindow(&m_showDemoWindow);
}

std::array<float, 4> EditorLayer::computeClearColor() const
{
    const float mix = std::clamp(m_backgroundMix, 0.0f, 1.0f);
    const std::array<float, 4> coolColor{0.08f, 0.12f, 0.18f, 1.0f};
    const std::array<float, 4> warmColor{0.22f, 0.13f, 0.08f, 1.0f};
    std::array<float, 4> clearColor{};
    for (size_t i = 0; i < 3; ++i) {
        const float base = m_warmBackground ? warmColor[i] : coolColor[i];
        const float accent = m_warmBackground ? coolColor[i] : warmColor[i];
        clearColor[i] = base * (1.0f - mix) + accent * mix;
    }
    clearColor[3] = 1.0f;
    return clearColor;
}

} // namespace luna::editor
