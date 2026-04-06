#include "Renderer/Vulkan/DeviceManager_VK.hpp"

namespace luna::renderer::vulkan {

bool DeviceManager_VK::initialize(GLFWwindow* window, const DeviceManagerCreateInfo& createInfo)
{
    VulkanContext::CreateInfo contextCreateInfo{};
    contextCreateInfo.appName = createInfo.appName;
    contextCreateInfo.engineName = createInfo.engineName;
    contextCreateInfo.apiVersion = createInfo.apiVersion != 0 ? createInfo.apiVersion : VK_API_VERSION_1_1;
    contextCreateInfo.enableValidation = createInfo.enableValidation;
    return m_context.initialize(window, contextCreateInfo);
}

void DeviceManager_VK::shutdown()
{
    m_context.shutdown();
}

} // namespace luna::renderer::vulkan
