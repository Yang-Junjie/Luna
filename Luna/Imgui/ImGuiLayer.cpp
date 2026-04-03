#include "Imgui/ImGuiLayer.hpp"

#include "Core/log.h"
#include "Events/event.h"
#include "Vulkan/vk_engine.h"
#include "Vulkan/vk_types.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <cstdlib>

namespace luna {

namespace {

constexpr uint32_t kImGuiDescriptorPoolSize = 64;

} // namespace

ImGuiLayer::ImGuiLayer(GLFWwindow* window, VulkanEngine& engine, bool enableMultiViewport)
    : Layer("ImGuiLayer"),
      m_enableMultiViewport(enableMultiViewport),
      m_window(window),
      m_engine(&engine)
{}

void ImGuiLayer::onAttach()
{
    if (m_attached) {
        return;
    }

    if (m_window == nullptr || m_engine == nullptr || !m_engine->_device ||
        m_engine->_swapchainImageFormat == vk::Format::eUndefined) {
        LUNA_CORE_ERROR("Cannot initialize ImGui layer because Vulkan state is incomplete");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (m_enableMultiViewport) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }
    io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    setDarkThemeColors();
    setImGuiWidgetStyle();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    if (!ImGui_ImplGlfw_InitForVulkan(m_window, true)) {
        LUNA_CORE_ERROR("Failed to initialize ImGui GLFW backend for Vulkan");
        ImGui::DestroyContext();
        return;
    }

    m_colorAttachmentFormat = m_engine->getSwapchainImageFormat();
    const uint32_t imageCount = m_engine->getSwapchainImageCount();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = static_cast<VkInstance>(m_engine->_instance);
    initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(m_engine->_chosenGPU);
    initInfo.Device = static_cast<VkDevice>(m_engine->_device);
    initInfo.QueueFamily = m_engine->_graphicsQueueFamily;
    initInfo.Queue = static_cast<VkQueue>(m_engine->_graphicsQueue);
    initInfo.DescriptorPoolSize = kImGuiDescriptorPoolSize;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.MinAllocationSize = 1024 * 1024;
    initInfo.UseDynamicRendering = true;
    initInfo.CheckVkResultFn = &ImGuiLayer::checkVulkanResult;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoForViewports.Subpass = 0;
    initInfo.PipelineInfoForViewports.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoForViewports.SwapChainImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    const VkFormat colorAttachmentFormat = static_cast<VkFormat>(m_colorAttachmentFormat);
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;
    initInfo.PipelineInfoForViewports.PipelineRenderingCreateInfo = pipelineRenderingInfo;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        LUNA_CORE_ERROR("Failed to initialize ImGui Vulkan backend");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return;
    }

    ImGui_ImplGlfw_SetCallbacksChainForAllWindows(true);
    m_attached = true;
    LUNA_CORE_INFO("Initialized ImGui for Vulkan");
}

void ImGuiLayer::onDetach()
{
    if (!m_attached) {
        return;
    }

    if (m_engine != nullptr && m_engine->_device) {
        VK_CHECK(m_engine->_device.waitIdle());
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_colorAttachmentFormat = vk::Format::eUndefined;
    m_attached = false;
}

void ImGuiLayer::onEvent(Event& event)
{
    if (!m_attached || !m_blockEvents) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    event.handled |= event.isInCategory(EventCategory::EventCategoryMouse) && io.WantCaptureMouse;
    event.handled |= event.isInCategory(EventCategory::EventCategoryKeyboard) && io.WantCaptureKeyboard;
}

void ImGuiLayer::begin()
{
    if (!m_attached) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }
}

void ImGuiLayer::end()
{
    if (!m_attached) {
        return;
    }

    ImGui::Render();
}

void ImGuiLayer::render(vk::CommandBuffer commandBuffer, vk::ImageView targetImageView, vk::Extent2D targetExtent)
{
    if (!m_attached) {
        return;
    }

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData == nullptr || drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f) {
        return;
    }

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView = targetImageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.extent = targetExtent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    commandBuffer.beginRendering(&renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(drawData, static_cast<VkCommandBuffer>(commandBuffer));
    commandBuffer.endRendering();
}

void ImGuiLayer::renderPlatformWindows()
{
    if (!m_attached || !viewportsEnabled()) {
        return;
    }

    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

bool ImGuiLayer::viewportsEnabled() const
{
    if (!m_attached) {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    return (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

void ImGuiLayer::checkVulkanResult(VkResult result)
{
    if (result == VK_SUCCESS) {
        return;
    }

    if (result < 0) {
        LUNA_CORE_FATAL("ImGui Vulkan backend error: {}", vk::to_string(static_cast<vk::Result>(result)));
        std::abort();
    }

    LUNA_CORE_WARN("ImGui Vulkan backend warning: {}", vk::to_string(static_cast<vk::Result>(result)));
}

void ImGuiLayer::setImGuiWidgetStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScrollbarRounding = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
}

void ImGuiLayer::setDarkThemeColors()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4{0.11f, 0.11f, 0.11f, 1.00f};
    colors[ImGuiCol_ChildBg] = ImVec4{0.11f, 0.11f, 0.11f, 1.00f};
    colors[ImGuiCol_PopupBg] = ImVec4{0.08f, 0.08f, 0.08f, 0.96f};
    colors[ImGuiCol_Border] = ImVec4{0.17f, 0.17f, 0.18f, 1.00f};

    colors[ImGuiCol_TitleBg] = ImVec4{0.07f, 0.07f, 0.07f, 1.00f};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.09f, 0.09f, 0.09f, 1.00f};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.07f, 0.07f, 0.07f, 1.00f};

    colors[ImGuiCol_FrameBg] = ImVec4{0.16f, 0.16f, 0.17f, 1.00f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.22f, 0.22f, 0.23f, 1.00f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.13f, 0.13f, 0.14f, 1.00f};

    const ImVec4 orangeMain = ImVec4{0.92f, 0.45f, 0.11f, 1.00f};
    const ImVec4 orangeHovered = ImVec4{1.00f, 0.55f, 0.20f, 1.00f};
    const ImVec4 orangeActive = ImVec4{0.80f, 0.38f, 0.08f, 1.00f};

    colors[ImGuiCol_Button] = ImVec4{0.20f, 0.20f, 0.21f, 1.00f};
    colors[ImGuiCol_ButtonHovered] = orangeMain;
    colors[ImGuiCol_ButtonActive] = orangeActive;

    colors[ImGuiCol_Tab] = ImVec4{0.12f, 0.12f, 0.13f, 1.00f};
    colors[ImGuiCol_TabHovered] = orangeHovered;
    colors[ImGuiCol_TabActive] = orangeMain;
    colors[ImGuiCol_TabUnfocused] = ImVec4{0.12f, 0.12f, 0.13f, 1.00f};
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.18f, 0.18f, 0.19f, 1.00f};

    colors[ImGuiCol_CheckMark] = orangeMain;
    colors[ImGuiCol_SliderGrab] = orangeMain;
    colors[ImGuiCol_SliderGrabActive] = orangeActive;
    colors[ImGuiCol_Header] = ImVec4{0.35f, 0.20f, 0.08f, 0.50f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.92f, 0.45f, 0.11f, 0.30f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.92f, 0.45f, 0.11f, 0.50f};
    colors[ImGuiCol_TextSelectedBg] = ImVec4{0.92f, 0.45f, 0.11f, 0.35f};
    colors[ImGuiCol_SeparatorHovered] = orangeMain;
    colors[ImGuiCol_SeparatorActive] = orangeActive;
    colors[ImGuiCol_ResizeGrip] = ImVec4{0.92f, 0.45f, 0.11f, 0.20f};
    colors[ImGuiCol_ResizeGripHovered] = orangeMain;
    colors[ImGuiCol_ResizeGripActive] = orangeActive;

    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.TabRounding = 2.0f;
}

} // namespace luna
