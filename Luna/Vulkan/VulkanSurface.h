#pragma once

#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace luna::val {
class VulkanContext;
struct WindowSurface;

const WindowSurface& CreateWindowSurface(GLFWwindow* window, const VulkanContext& context);
bool CheckVulkanPresentationSupport(const vk::Instance& instance,
                                    const vk::PhysicalDevice& physicalDevice,
                                    uint32_t familyQueueIndex);
} // namespace luna::val
