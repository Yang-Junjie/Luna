#include "Imgui/ImGuiLayer.hpp"

#include "Core/Log.h"
#include "Events/Event.h"
#include "Vulkan/VkEngine.h"
#include "Vulkan/VkTypes.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <cstdlib>

namespace luna {

namespace {

constexpr uint32_t k_im_gui_descriptor_pool_size = 64;

} // namespace

ImGuiLayer::ImGuiLayer(GLFWwindow* window, VulkanEngine& engine, bool enable_multi_viewport)
    : Layer("ImGuiLayer"),
      m_enable_multi_viewport(enable_multi_viewport),
      m_window(window),
      m_engine(&engine)
{}

void ImGuiLayer::onAttach()
{
    if (m_attached) {
        return;
    }

    if (m_window == nullptr || m_engine == nullptr || !m_engine->hasDevice() ||
        m_engine->getSwapchainImageFormat() == luna::render::PixelFormat::Undefined) {
        LUNA_CORE_ERROR("Cannot initialize ImGui layer because Vulkan state is incomplete");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (m_enable_multi_viewport) {
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

    m_color_attachment_format = m_engine->getSwapchainImageFormat();
    const uint32_t image_count = m_engine->getSwapchainImageCount();

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = static_cast<VkInstance>(m_engine->getInstanceHandle());
    init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(m_engine->getPhysicalDeviceHandle());
    init_info.Device = static_cast<VkDevice>(m_engine->getDeviceHandle());
    init_info.QueueFamily = m_engine->getGraphicsQueueFamily();
    init_info.Queue = static_cast<VkQueue>(m_engine->getGraphicsQueueHandle());
    init_info.DescriptorPoolSize = k_im_gui_descriptor_pool_size;
    init_info.MinImageCount = image_count;
    init_info.ImageCount = image_count;
    init_info.MinAllocationSize = 1024 * 1024;
    init_info.UseDynamicRendering = true;
    init_info.CheckVkResultFn = &ImGuiLayer::checkVulkanResult;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoForViewports.Subpass = 0;
    init_info.PipelineInfoForViewports.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoForViewports.SwapChainImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkPipelineRenderingCreateInfo pipeline_rendering_info{};
    pipeline_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipeline_rendering_info.colorAttachmentCount = 1;
    const VkFormat color_attachment_format = static_cast<VkFormat>(toVk(m_color_attachment_format));
    pipeline_rendering_info.pColorAttachmentFormats = &color_attachment_format;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_info;
    init_info.PipelineInfoForViewports.PipelineRenderingCreateInfo = pipeline_rendering_info;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
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

    if (m_engine != nullptr && m_engine->hasDevice()) {
        VK_CHECK(m_engine->getDeviceHandle().waitIdle());
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_color_attachment_format = luna::render::PixelFormat::Undefined;
    m_attached = false;
}

void ImGuiLayer::onEvent(Event& event)
{
    if (!m_attached || !m_block_events) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    event.m_handled |= event.isInCategory(EventCategory::EventCategoryMouse) && io.WantCaptureMouse;
    event.m_handled |= event.isInCategory(EventCategory::EventCategoryKeyboard) && io.WantCaptureKeyboard;
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

void ImGuiLayer::render(RenderCommandList& command_list,
                        const luna::vkcore::ImageView& target_image_view,
                        luna::render::Extent2D target_extent)
{
    if (!m_attached) {
        return;
    }

    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) {
        return;
    }

    if (!command_list.isValid()) {
        return;
    }

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView = target_image_view.get();
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.loadOp = vk::AttachmentLoadOp::eLoad;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;

    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.extent = toVk(target_extent);
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    command_list.m_handle.beginRendering(&rendering_info);
    ImGui_ImplVulkan_RenderDrawData(draw_data, static_cast<VkCommandBuffer>(command_list.m_handle));
    command_list.m_handle.endRendering();
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

    const ImVec4 orange_main = ImVec4{0.92f, 0.45f, 0.11f, 1.00f};
    const ImVec4 orange_hovered = ImVec4{1.00f, 0.55f, 0.20f, 1.00f};
    const ImVec4 orange_active = ImVec4{0.80f, 0.38f, 0.08f, 1.00f};

    colors[ImGuiCol_Button] = ImVec4{0.20f, 0.20f, 0.21f, 1.00f};
    colors[ImGuiCol_ButtonHovered] = orange_main;
    colors[ImGuiCol_ButtonActive] = orange_active;

    colors[ImGuiCol_Tab] = ImVec4{0.12f, 0.12f, 0.13f, 1.00f};
    colors[ImGuiCol_TabHovered] = orange_hovered;
    colors[ImGuiCol_TabActive] = orange_main;
    colors[ImGuiCol_TabUnfocused] = ImVec4{0.12f, 0.12f, 0.13f, 1.00f};
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.18f, 0.18f, 0.19f, 1.00f};

    colors[ImGuiCol_CheckMark] = orange_main;
    colors[ImGuiCol_SliderGrab] = orange_main;
    colors[ImGuiCol_SliderGrabActive] = orange_active;
    colors[ImGuiCol_Header] = ImVec4{0.35f, 0.20f, 0.08f, 0.50f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.92f, 0.45f, 0.11f, 0.30f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.92f, 0.45f, 0.11f, 0.50f};
    colors[ImGuiCol_TextSelectedBg] = ImVec4{0.92f, 0.45f, 0.11f, 0.35f};
    colors[ImGuiCol_SeparatorHovered] = orange_main;
    colors[ImGuiCol_SeparatorActive] = orange_active;
    colors[ImGuiCol_ResizeGrip] = ImVec4{0.92f, 0.45f, 0.11f, 0.20f};
    colors[ImGuiCol_ResizeGripHovered] = orange_main;
    colors[ImGuiCol_ResizeGripActive] = orange_active;

    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.TabRounding = 2.0f;
}

} // namespace luna

