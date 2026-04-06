#include "Renderer/Vulkan/VulkanContext.hpp"

#include "Core/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace luna::renderer::vulkan {

namespace {

constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                  void*)
{
    const char* message = callbackData != nullptr && callbackData->pMessage != nullptr ? callbackData->pMessage
                                                                                        : "Unknown validation message";

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LUNA_CORE_ERROR("Validation: {}", message);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LUNA_CORE_WARN("Validation: {}", message);
    } else {
        LUNA_CORE_INFO("Validation: {}", message);
    }

    return VK_FALSE;
}

} // namespace

void logVkResult(VkResult result)
{
    if (result != VK_SUCCESS) {
        LUNA_CORE_ERROR("Vulkan call failed with VkResult={}", static_cast<int>(result));
    }
}

bool VulkanContext::initialize(GLFWwindow* window, const CreateInfo& createInfo)
{
    m_validationEnabled = createInfo.enableValidation;

    if (!createInstance(createInfo) || !createSurface(window) || !selectPhysicalDevice() || !createLogicalDevice()) {
        shutdown();
        return false;
    }

    return true;
}

void VulkanContext::shutdown()
{
    if (m_device.device != VK_NULL_HANDLE) {
        vkb::destroy_device(m_device);
        m_device = {};
    }

    if (m_surface != VK_NULL_HANDLE && m_instance.instance != VK_NULL_HANDLE) {
        vkb::destroy_surface(m_instance, m_surface);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_instance.instance != VK_NULL_HANDLE) {
        vkb::destroy_instance(m_instance);
        m_instance = {};
    }

    m_physicalDeviceSelection = {};
    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_presentQueue = VK_NULL_HANDLE;
    m_graphicsQueueFamily = UINT32_MAX;
    m_presentQueueFamily = UINT32_MAX;
    m_gpuName.clear();
    m_requiredInstanceExtensions.clear();
    m_validationEnabled = false;
}

bool VulkanContext::createInstance(const CreateInfo& createInfo)
{
    std::uint32_t requiredExtensionCount = 0;
    const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
    if (requiredExtensions == nullptr || requiredExtensionCount == 0) {
        LUNA_CORE_ERROR("GLFW did not return required Vulkan instance extensions");
        return false;
    }

    m_requiredInstanceExtensions.clear();
    for (std::uint32_t i = 0; i < requiredExtensionCount; ++i) {
        m_requiredInstanceExtensions.emplace_back(requiredExtensions[i]);
        LUNA_CORE_INFO("GLFW required Vulkan extension: {}", requiredExtensions[i]);
    }

    if (m_validationEnabled) {
        LUNA_CORE_INFO("Debug validation layer target: {}", kValidationLayerName);
    } else {
        LUNA_CORE_INFO("Debug validation layer target: disabled in this build");
    }

    vkb::InstanceBuilder builder;
    builder.set_app_name(createInfo.appName)
        .set_engine_name(createInfo.engineName)
        .require_api_version(VK_API_VERSION_MAJOR(createInfo.apiVersion),
                             VK_API_VERSION_MINOR(createInfo.apiVersion),
                             VK_API_VERSION_PATCH(createInfo.apiVersion))
        .enable_extensions(requiredExtensionCount, requiredExtensions);

    if (m_validationEnabled) {
        builder.request_validation_layers()
            .set_debug_callback(debugUtilsCallback)
            .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    }

    const auto instanceResult = builder.build();
    if (!instanceResult) {
        LUNA_CORE_ERROR("Failed to create Vulkan instance");
        return false;
    }

    m_instance = instanceResult.value();
    LUNA_CORE_INFO("Vulkan instance created");
    if (m_instance.debug_messenger != VK_NULL_HANDLE) {
        LUNA_CORE_INFO("Debug messenger created");
    }

    return true;
}

bool VulkanContext::createSurface(GLFWwindow* window)
{
    if (window == nullptr) {
        LUNA_CORE_ERROR("Cannot create Vulkan surface without a GLFW window");
        return false;
    }

    if (glfwCreateWindowSurface(m_instance.instance, window, nullptr, &m_surface) != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create Vulkan surface from GLFW window");
        return false;
    }

    LUNA_CORE_INFO("Surface created");
    return true;
}

bool VulkanContext::selectPhysicalDevice()
{
    vkb::PhysicalDeviceSelector selector(m_instance, m_surface);
    const auto physicalDeviceResult =
        selector.set_minimum_version(1, 1).add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME).select();
    if (!physicalDeviceResult) {
        LUNA_CORE_ERROR("Failed to select a Vulkan physical device");
        return false;
    }

    m_physicalDeviceSelection = physicalDeviceResult.value();
    m_physicalDevice = m_physicalDeviceSelection.physical_device;
    m_gpuName = m_physicalDeviceSelection.properties.deviceName;
    LUNA_CORE_INFO("Selected GPU: {}", m_gpuName);
    return true;
}

bool VulkanContext::createLogicalDevice()
{
    vkb::DeviceBuilder deviceBuilder(m_physicalDeviceSelection);
    const auto deviceResult = deviceBuilder.build();
    if (!deviceResult) {
        LUNA_CORE_ERROR("Failed to create Vulkan logical device");
        return false;
    }

    m_device = deviceResult.value();

    const auto graphicsQueueFamily = m_device.get_queue_index(vkb::QueueType::graphics);
    const auto presentQueueFamily = m_device.get_queue_index(vkb::QueueType::present);
    const auto graphicsQueue = m_device.get_queue(vkb::QueueType::graphics);
    const auto presentQueue = m_device.get_queue(vkb::QueueType::present);
    if (!graphicsQueueFamily || !presentQueueFamily || !graphicsQueue || !presentQueue) {
        LUNA_CORE_ERROR("Failed to query required Vulkan queues");
        return false;
    }

    m_graphicsQueueFamily = graphicsQueueFamily.value();
    m_presentQueueFamily = presentQueueFamily.value();
    m_graphicsQueue = graphicsQueue.value();
    m_presentQueue = presentQueue.value();
    m_physicalDevice = m_device.physical_device.physical_device;
    m_gpuName = m_device.physical_device.properties.deviceName;

    LUNA_CORE_INFO("Graphics queue family: {}", m_graphicsQueueFamily);
    LUNA_CORE_INFO("Present queue family: {}", m_presentQueueFamily);
    LUNA_CORE_INFO("Logical Device Ready");
    return true;
}

} // namespace luna::renderer::vulkan
