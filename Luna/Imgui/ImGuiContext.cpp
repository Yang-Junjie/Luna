#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "Core/Log.h"
#include "Imgui/ImGuiContext.h"
#include "Renderer/Renderer.h"

#include <cstdint>

#include <algorithm>
#include <Barrier.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <Device.h>
#include <Impls/Vulkan/VKAdapter.h>
#include <Impls/Vulkan/VKCommon.h>
#include <Impls/Vulkan/VKDevice.h>
#include <Impls/Vulkan/VKInstance.h>
#include <Impls/Vulkan/VKQueue.h>
#include <Impls/Vulkan/VKSampler.h>
#include <Impls/Vulkan/VKTexture.h>
#include <Texture.h>
#include <vulkan/vulkan.hpp>

namespace luna::rhi {
namespace {

constexpr uint32_t kImGuiDescriptorPoolSize = 256;

Cacao::Ref<Cacao::Device> g_device;
Cacao::Ref<Cacao::Sampler> g_default_sampler;
bool g_initialized = false;

void checkVkResult(VkResult result)
{
    if (result != VK_SUCCESS) {
        LUNA_CORE_ERROR("ImGui Vulkan backend returned error {}", static_cast<int>(result));
    }
}

} // namespace

bool ImGuiVulkanContext::Init(luna::Renderer& renderer)
{
    if (g_initialized || !renderer.isInitialized() || renderer.getNativeWindow() == nullptr ||
        !renderer.getSwapchain()) {
        return g_initialized;
    }

    const auto vk_instance = std::dynamic_pointer_cast<Cacao::VKInstance>(renderer.getInstance());
    const auto vk_adapter = std::dynamic_pointer_cast<Cacao::VKAdapter>(renderer.getAdapter());
    const auto vk_device = std::dynamic_pointer_cast<Cacao::VKDevice>(renderer.getDevice());
    const auto vk_queue = std::dynamic_pointer_cast<Cacao::VKQueue>(renderer.getGraphicsQueue());
    if (!vk_instance || !vk_adapter || !vk_device || !vk_queue) {
        LUNA_CORE_ERROR("Cannot initialize ImGui because Vulkan native handles are unavailable");
        return false;
    }

    const vk::Format color_attachment_format = Cacao::VKConverter::Convert(renderer.getSwapchain()->GetFormat());
    const VkFormat color_attachment_vk_format = static_cast<VkFormat>(color_attachment_format);

    ImGui_ImplGlfw_InitForVulkan(renderer.getNativeWindow(), true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = static_cast<VkInstance>(vk_instance->GetNativeHandle());
    init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(vk_adapter->GetNativeHandle());
    init_info.Device = static_cast<VkDevice>(vk_device->GetNativeHandle());
    init_info.QueueFamily = renderer.getAdapter()->FindQueueFamilyIndex(Cacao::QueueType::Graphics);
    init_info.Queue = static_cast<VkQueue>(vk_queue->GetNativeHandle());
    init_info.DescriptorPool = VK_NULL_HANDLE;
    init_info.DescriptorPoolSize = kImGuiDescriptorPoolSize;
    init_info.MinImageCount = (std::max) (2u, renderer.getSwapchain()->GetImageCount());
    init_info.ImageCount = renderer.getSwapchain()->GetImageCount();
    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_attachment_vk_format;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = checkVkResult;
    ImGui_ImplVulkan_Init(&init_info);

    g_device = renderer.getDevice();
    g_default_sampler = g_device->CreateSampler(Cacao::SamplerBuilder()
                                                    .SetFilter(Cacao::Filter::Linear, Cacao::Filter::Linear)
                                                    .SetAddressMode(Cacao::SamplerAddressMode::ClampToEdge)
                                                    .SetMipmapMode(Cacao::SamplerMipmapMode::Linear)
                                                    .SetAnisotropy(false)
                                                    .SetName("ImGuiDefaultSampler")
                                                    .Build());

    g_initialized = true;
    return true;
}

void ImGuiVulkanContext::Destroy()
{
    if (!g_initialized) {
        return;
    }

    if (g_device) {
        try {
            if (const auto vk_device = std::dynamic_pointer_cast<Cacao::VKDevice>(g_device)) {
                vk_device->GetNativeHandle().waitIdle();
            }
        } catch (...) {
        }
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    g_default_sampler.reset();
    g_device.reset();
    g_initialized = false;
}

void ImGuiVulkanContext::StartFrame()
{
    if (!g_initialized) {
        return;
    }

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
}

void ImGuiVulkanContext::RenderFrame(Cacao::CommandBufferEncoder& command_buffer,
                                     const Cacao::Ref<Cacao::Texture>& color_target,
                                     uint32_t framebuffer_width,
                                     uint32_t framebuffer_height)
{
    if (!g_initialized || !color_target || framebuffer_width == 0 || framebuffer_height == 0) {
        return;
    }

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr) {
        return;
    }

    Cacao::RenderingAttachmentInfo color_attachment;
    color_attachment.Texture = color_target;
    color_attachment.LoadOp = Cacao::AttachmentLoadOp::Load;
    color_attachment.StoreOp = Cacao::AttachmentStoreOp::Store;

    Cacao::RenderingInfo rendering_info;
    rendering_info.RenderArea = {0, 0, framebuffer_width, framebuffer_height};
    rendering_info.ColorAttachments = {color_attachment};
    rendering_info.LayerCount = 1;

    command_buffer.BeginRendering(rendering_info);
    auto* native_command_buffer = static_cast<vk::CommandBuffer*>(command_buffer.GetNativeHandle());
    ImGui_ImplVulkan_RenderDrawData(draw_data, static_cast<VkCommandBuffer>(*native_command_buffer));
    command_buffer.EndRendering();
}

ImTextureID ImGuiVulkanContext::GetTextureId(const Cacao::Ref<Cacao::Texture>& texture)
{
    if (!texture) {
        return {};
    }
    return GetTextureId(texture->GetDefaultView(), g_default_sampler);
}

ImTextureID ImGuiVulkanContext::GetTextureId(const Cacao::Ref<Cacao::CacaoTextureView>& view,
                                             const Cacao::Ref<Cacao::Sampler>& sampler)
{
    if (!g_initialized || !view) {
        return {};
    }

    const auto vk_view = std::dynamic_pointer_cast<Cacao::VKTextureView>(view);
    const auto vk_sampler = std::dynamic_pointer_cast<Cacao::VKSampler>(sampler ? sampler : g_default_sampler);
    if (!vk_view || !vk_sampler) {
        return {};
    }

    const auto descriptor_set = ImGui_ImplVulkan_AddTexture(
        vk_sampler->GetHandle(), vk_view->GetHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(descriptor_set));
}

void ImGuiVulkanContext::EndFrame() {}

void ImGuiVulkanContext::NotifySwapchainChanged(uint32_t image_count)
{
    if (!g_initialized) {
        return;
    }

    ImGui_ImplVulkan_SetMinImageCount((std::max) (2u, image_count));
}

} // namespace luna::rhi
