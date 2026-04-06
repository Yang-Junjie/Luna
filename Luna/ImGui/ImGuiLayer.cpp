#include "ImGui/ImGuiLayer.hpp"

#include "Core/log.h"
#include "Renderer/Vulkan/VulkanContext.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <algorithm>

namespace luna {

ImGuiLayer::~ImGuiLayer()
{
    shutdown();
}

bool ImGuiLayer::initialize(const ImGuiVulkanBackendConfig& config)
{
    shutdown();

    if (config.window == nullptr || config.instance == VK_NULL_HANDLE || config.physicalDevice == VK_NULL_HANDLE ||
        config.device == VK_NULL_HANDLE || config.queue == VK_NULL_HANDLE || config.renderPass == VK_NULL_HANDLE ||
        config.imageCount < 2) {
        LUNA_CORE_ERROR("ImGui initialization received incomplete Vulkan backend configuration");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (!ImGui_ImplGlfw_InitForVulkan(config.window, true)) {
        LUNA_CORE_ERROR("Failed to initialize ImGui GLFW backend");
        ImGui::DestroyContext();
        return false;
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = config.apiVersion;
    initInfo.Instance = config.instance;
    initInfo.PhysicalDevice = config.physicalDevice;
    initInfo.Device = config.device;
    initInfo.QueueFamily = config.queueFamily;
    initInfo.Queue = config.queue;
    initInfo.DescriptorPool = VK_NULL_HANDLE;
    initInfo.DescriptorPoolSize = 32;
    initInfo.MinImageCount = std::max(2u, config.minImageCount);
    initInfo.ImageCount = config.imageCount;
    initInfo.PipelineInfoMain.RenderPass = config.renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = false;
    initInfo.CheckVkResultFn = renderer::vulkan::logVkResult;
    initInfo.MinAllocationSize = 1024 * 1024;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        LUNA_CORE_ERROR("Failed to initialize ImGui Vulkan backend");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_initialized = true;
    LUNA_CORE_INFO("ImGui backend ready");
    return true;
}

void ImGuiLayer::shutdown()
{
    if (m_initialized) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }

    m_initialized = false;
}

void ImGuiLayer::beginFrame()
{
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame()
{
    if (!m_initialized) {
        return;
    }

    ImGui::Render();
}

void ImGuiLayer::onEvent(Event& event)
{
    if (!m_initialized) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    event.handled |= event.isInCategory(EventCategory::EventCategoryMouse) && io.WantCaptureMouse;
    event.handled |= event.isInCategory(EventCategory::EventCategoryKeyboard) && io.WantCaptureKeyboard;
}

void ImGuiLayer::setMinImageCount(std::uint32_t minImageCount)
{
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_SetMinImageCount(std::max(2u, minImageCount));
}

} // namespace luna
