#pragma once

#include "Core/window.h"
#include "vk_engine_types.h"

#include <functional>
#include <string>
#include <string_view>

struct GLFWwindow;

class VulkanDeviceContext {
public:
    bool _isInitialized{false};
    int _frameNumber{0};
    vk::Extent2D _windowExtent{1'700, 900};
    bool resize_requested{false};

    GLFWwindow* _window{nullptr};

    bool init(luna::Window& window);
    bool init_headless();
    void cleanup();

    void setApplicationName(std::string_view applicationName)
    {
        m_applicationName = applicationName.empty() ? "Luna" : std::string(applicationName);
    }

    const std::string& getApplicationName() const
    {
        return m_applicationName;
    }

    void setPreferredAdapterId(uint64_t adapterId)
    {
        m_preferredAdapterId = adapterId;
    }

    uint64_t getPreferredAdapterId() const
    {
        return m_preferredAdapterId;
    }

    void request_swapchain_resize()
    {
        resize_requested = true;
    }

    bool is_swapchain_resize_requested() const
    {
        return resize_requested;
    }

    bool resize_swapchain();

    void setSwapchainDesc(const luna::SwapchainDesc& desc)
    {
        m_requestedSwapchainDesc = desc;
    }

    const luna::SwapchainDesc& getRequestedSwapchainDesc() const
    {
        return m_requestedSwapchainDesc;
    }

    void begin_upload_batch(vk::CommandBuffer commandBuffer, FrameData& frame);
    void end_upload_batch();
    bool has_active_upload_batch() const;

    uint32_t getSwapchainImageCount() const
    {
        return static_cast<uint32_t>(_swapchainImages.size());
    }

    bool hasSwapchain() const
    {
        return _swapchain != VK_NULL_HANDLE;
    }

    uint64_t getSwapchainGeneration() const
    {
        return m_swapchainGeneration;
    }

    vk::Format getSwapchainImageFormat() const
    {
        return _swapchainImageFormat;
    }

    vk::PresentModeKHR getSwapchainPresentMode() const
    {
        return _swapchainPresentMode;
    }

    bool isSamplerAnisotropyEnabled() const
    {
        return m_samplerAnisotropyEnabled;
    }

    FrameData _frames[FRAME_OVERLAP];
    ImmediateSubmitContext _immContext;

    FrameData& get_current_frame()
    {
        return _frames[_frameNumber % FRAME_OVERLAP];
    }

    vk::Instance _instance{};
    vk::DebugUtilsMessengerEXT _debug_messenger{};
    vk::PhysicalDevice _chosenGPU{};
    vk::Device _device{};
    vk::SurfaceKHR _surface{};
    vk::Queue _graphicsQueue{};
    uint32_t _graphicsQueueFamily{0};

    vk::SwapchainKHR _swapchain{};
    vk::Format _swapchainImageFormat{vk::Format::eUndefined};
    vk::PresentModeKHR _swapchainPresentMode{vk::PresentModeKHR::eFifo};

    std::vector<vk::Image> _swapchainImages;
    std::vector<vk::ImageView> _swapchainImageViews;
    std::vector<VkImageLayout> _swapchainImageLayouts;
    vk::Extent2D _swapchainExtent{};

    DeletionQueue _mainDeletionQueue;
    VmaAllocator _allocator{VK_NULL_HANDLE};

    AllocatedImage create_image(vk::Extent3D size,
                                vk::Format format,
                                vk::ImageUsageFlags usage,
                                bool mipmapped = false);
    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false)
    {
        return create_image(to_vk(size),
                            static_cast<vk::Format>(format),
                            static_cast<vk::ImageUsageFlags>(usage),
                            mipmapped);
    }

    AllocatedImage create_image(
        void* data, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(
        void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false)
    {
        return create_image(data,
                            to_vk(size),
                            static_cast<vk::Format>(format),
                            static_cast<vk::ImageUsageFlags>(usage),
                            mipmapped);
    }

    AllocatedBuffer create_buffer(const luna::BufferDesc& desc, const void* initialData = nullptr);
    AllocatedImage create_image(const luna::ImageDesc& desc, const void* initialData = nullptr);
    vk::Sampler create_sampler(const luna::SamplerDesc& desc, vk::SamplerCreateInfo* outCreateInfo = nullptr);
    void destroy_image(const AllocatedImage& image);
    void destroy_buffer(const AllocatedBuffer& buffer);
    bool uploadBufferData(const AllocatedBuffer& buffer, const void* data, size_t size, size_t offset = 0);

protected:
    struct ActiveUploadContext {
        vk::CommandBuffer commandBuffer{};
        FrameData* frame = nullptr;
    };

    bool init_vulkan();
    bool init_swapchain();
    bool init_commands();
    bool init_sync_structures();

    bool create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();

    AllocatedBuffer create_buffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
    {
        return create_buffer(allocSize, static_cast<vk::BufferUsageFlags>(usage), memoryUsage);
    }
    AllocatedBuffer acquire_upload_staging_buffer(size_t size, size_t alignment, vk::DeviceSize* outOffset);
    void reset_frame_upload_state(FrameData& frame);
    void immediate_submit(const std::function<void(vk::CommandBuffer cmd)>& function);
    vk::Extent2D get_framebuffer_extent() const;

private:
    luna::SwapchainDesc m_requestedSwapchainDesc{};
    ActiveUploadContext m_activeUploadContext{};
    std::string m_applicationName = "Luna";
    uint64_t m_preferredAdapterId = 0;
    bool m_samplerAnisotropyEnabled = false;
    uint64_t m_swapchainGeneration = 0;
};
