#include "VkInstance.h"

#include <Core/Log.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <utility>

namespace {

template <typename T> void logVkbError(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

} // namespace

namespace luna::vkcore {

Instance::~Instance()
{
    destroy();
}

Instance::Instance(Instance&& other) noexcept
{
    *this = std::move(other);
}

Instance& Instance::operator=(Instance&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    destroy();

    m_vkb_instance = other.m_vkb_instance;
    m_instance = other.m_instance;
    m_surface = other.m_surface;
    m_debug_messenger = other.m_debug_messenger;

    other.m_vkb_instance = {};
    other.m_instance = VK_NULL_HANDLE;
    other.m_surface = VK_NULL_HANDLE;
    other.m_debug_messenger = VK_NULL_HANDLE;
    return *this;
}

bool Instance::create(const char* app_name, GLFWwindow* window, bool enable_validation_layers)
{
    destroy();

    if (window == nullptr) {
        LUNA_CORE_ERROR("Cannot create Vulkan instance without a GLFW window");
        return false;
    }

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    if (glfw_extensions == nullptr || glfw_extension_count == 0) {
        LUNA_CORE_ERROR("Failed to query GLFW Vulkan instance extensions");
        return false;
    }

    vkb::InstanceBuilder builder;
    auto instance_ret = builder.set_app_name(app_name != nullptr ? app_name : "Luna")
                            .request_validation_layers(enable_validation_layers)
                            .use_default_debug_messenger()
                            .enable_extensions(glfw_extension_count, glfw_extensions)
                            .require_api_version(1, 3, 0)
                            .build();
    if (!instance_ret) {
        logVkbError("Vulkan instance creation", instance_ret);
        return false;
    }

    m_vkb_instance = instance_ret.value();
    m_instance = m_vkb_instance.instance;
    m_debug_messenger = m_vkb_instance.debug_messenger;

    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    const VkResult surface_result = glfwCreateWindowSurface(m_instance, window, nullptr, &raw_surface);
    if (surface_result != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create Vulkan surface: {}", stringVkResult(surface_result));
        destroy();
        return false;
    }

    m_surface = raw_surface;
    return true;
}

void Instance::destroy()
{
    if (m_instance != VK_NULL_HANDLE && m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    m_surface = VK_NULL_HANDLE;

    if (m_instance != VK_NULL_HANDLE && m_debug_messenger != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(m_instance, m_debug_messenger);
    }
    m_debug_messenger = VK_NULL_HANDLE;

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }

    m_instance = VK_NULL_HANDLE;
    m_vkb_instance = {};
}

} // namespace luna::vkcore
