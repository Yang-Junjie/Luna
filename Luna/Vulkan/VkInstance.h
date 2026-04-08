#pragma once

#include "VkBootstrap.h"
#include "VkTypes.h"

struct GLFWwindow;

namespace luna::vkcore {

class Instance {
public:
    Instance() = default;
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    Instance(Instance&& other) noexcept;
    Instance& operator=(Instance&& other) noexcept;

    bool create(const char* app_name, GLFWwindow* window, bool enable_validation_layers);
    void destroy();

    bool isValid() const
    {
        return m_instance != VK_NULL_HANDLE;
    }

    vk::Instance get() const
    {
        return m_instance;
    }

    vk::SurfaceKHR getSurface() const
    {
        return m_surface;
    }

    vk::DebugUtilsMessengerEXT getDebugMessenger() const
    {
        return m_debug_messenger;
    }

    const vkb::Instance& getBootstrapInstance() const
    {
        return m_vkb_instance;
    }

private:
    vkb::Instance m_vkb_instance{};
    vk::Instance m_instance{};
    vk::SurfaceKHR m_surface{};
    vk::DebugUtilsMessengerEXT m_debug_messenger{};
};

} // namespace luna::vkcore
