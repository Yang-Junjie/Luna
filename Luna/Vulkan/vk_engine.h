#pragma once

#include "Core/window.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_types.h"

struct GLFWwindow;

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    // TODO(Yang) : need to be optimalized, because the std::funcation is too slow
    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct FrameData {
    VkSemaphore _swapchainSemaphore{VK_NULL_HANDLE};
    VkSemaphore _renderSemaphore{VK_NULL_HANDLE};
    VkFence _renderFence{VK_NULL_HANDLE};

    VkCommandPool _commandPool{VK_NULL_HANDLE};
    VkCommandBuffer _mainCommandBuffer{VK_NULL_HANDLE};

    DeletionQueue _deletionQueue;
};

struct ImmediateSubmitContext {
    VkFence _fence{VK_NULL_HANDLE};
    VkCommandPool _commandPool{VK_NULL_HANDLE};
    VkCommandBuffer _commandBuffer{VK_NULL_HANDLE};
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

class VulkanEngine {
public:
    using OverlayRenderFunction = std::function<void(VkCommandBuffer, VkImageView, VkExtent2D)>;
    using BeforePresentFunction = std::function<void()>;

    bool _isInitialized{false};
    int _frameNumber{0};
    VkExtent2D _windowExtent{1'700, 900};
    bool resize_requested{false};
    float renderScale{1.0f};

    GLFWwindow* _window{nullptr};

    static VulkanEngine& Get();

    bool init(luna::Window& window);
    void cleanup();
    void draw(const OverlayRenderFunction& overlayRenderer = {}, const BeforePresentFunction& beforePresent = {});
    void request_swapchain_resize()
    {
        resize_requested = true;
    }
    bool is_swapchain_resize_requested() const
    {
        return resize_requested;
    }
    bool resize_swapchain();

    uint32_t getSwapchainImageCount() const
    {
        return static_cast<uint32_t>(_swapchainImages.size());
    }

    VkFormat getSwapchainImageFormat() const
    {
        return _swapchainImageFormat;
    }

    FrameData _frames[FRAME_OVERLAP];
    ImmediateSubmitContext _immContext;

    FrameData& get_current_frame()
    {
        return _frames[_frameNumber % FRAME_OVERLAP];
    }

    // temporary
    std::vector<ComputeEffect>& get_background_effects()
    {
        return backgroundEffects;
    }

    // temporary
    int& get_current_background_effect()
    {
        return currentBackgroundEffect;
    }

    const int& get_current_background_effect() const
    {
        return currentBackgroundEffect;
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
    std::vector<VkImageLayout> _swapchainImageLayouts;
    VkExtent2D _swapchainExtent{};

    DeletionQueue _mainDeletionQueue;

    VmaAllocator _allocator{VK_NULL_HANDLE};

    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent{};
    VkImageLayout _drawImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout _depthImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};

    DescriptorAllocator globalDescriptorAllocator;

    VkDescriptorSet _drawImageDescriptors{VK_NULL_HANDLE};
    VkDescriptorSetLayout _drawImageDescriptorLayout{VK_NULL_HANDLE};
    uint32_t _drawImageDescriptorBinding{0};
    uint32_t _drawImageDescriptorCount{1};
    VkDescriptorType _drawImageDescriptorType{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};

    VkPipeline _gradientPipeline{VK_NULL_HANDLE};
    VkPipelineLayout _gradientPipelineLayout{VK_NULL_HANDLE};

    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect{0};

    VkPipelineLayout _trianglePipelineLayout;
    VkPipeline _trianglePipeline;

    VkPipelineLayout _meshPipelineLayout;
    VkPipeline _meshPipeline;

    GPUMeshBuffers rectangle;
    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
    bool init_vulkan();
    bool init_swapchain();
    bool init_commands();
    bool init_sync_structures();
    bool init_descriptors();
    bool init_pipelines();
    bool init_background_pipelines();
    void init_triangle_pipeline();
    void init_mesh_pipeline();
    void init_default_data();

    bool create_swapchain(uint32_t width, uint32_t height);
    bool create_draw_resources(VkExtent2D extent);
    void destroy_draw_resources();

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroy_buffer(const AllocatedBuffer& buffer);
    void immediate_submit(const std::function<void(VkCommandBuffer cmd)>& function);

    void update_draw_image_descriptors();
    VkExtent2D get_framebuffer_extent() const;

    void draw_background(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);

    void destroy_swapchain();
};
