#pragma once

#include "vk_types.h"

#include <GLFW/glfw3.h>

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
    bool stop_rendering{false};
    VkExtent2D _windowExtent{1'700, 900};

    struct GLFWwindow* _window{nullptr};

    static VulkanEngine& Get();

    void init();
    void cleanup();
    void draw();
    void run();

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
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    VkExtent2D _swapchainExtent;

private:
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
};
