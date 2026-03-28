#pragma once

#include "Core/window.h"
#include "vk_types.h"

struct GLFWwindow;

struct FrameData {
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
    bool _isInitialized{false};
    int _frameNumber{0};
    VkExtent2D _windowExtent{1'700, 900};

    GLFWwindow* _window{nullptr};

    static VulkanEngine& Get();

    bool init(luna::Window& window);
    void cleanup();
    void draw();

    FrameData _frames[FRAME_OVERLAP];

    FrameData& get_current_frame()
    {
        return _frames[_frameNumber % FRAME_OVERLAP];
    }

    VkInstance _instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT _debug_messenger{VK_NULL_HANDLE};
    VkPhysicalDevice _chosenGPU{VK_NULL_HANDLE};
    VkDevice _device{VK_NULL_HANDLE};
    VkSurfaceKHR _surface{VK_NULL_HANDLE};
    VkQueue _graphicsQueue{VK_NULL_HANDLE};
    uint32_t _graphicsQueueFamily{0};

    VkSwapchainKHR _swapchain{VK_NULL_HANDLE};
    VkFormat _swapchainImageFormat{VK_FORMAT_UNDEFINED};

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    VkExtent2D _swapchainExtent{};

private:
    bool init_vulkan();
    bool init_swapchain();
    bool init_commands();
    bool init_sync_structures();
    bool create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
};
