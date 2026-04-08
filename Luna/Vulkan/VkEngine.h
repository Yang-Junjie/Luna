#pragma once

#include "Core/Window.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderContext.h"
#include "Renderer/VkLoader.h"
#include "VkContext.h"
#include "VkDeferredRelease.h"
#include "VkDescriptors.h"
#include "VkFrameContext.h"
#include "VkRenderTarget.h"
#include "VkTypes.h"
#include "VkVMAAllocator.h"

#include <unordered_map>

struct GLFWwindow;
class VulkanEngine;

struct ImmediateSubmitContext {
    vk::Fence m_fence{};
    vk::CommandPool m_command_pool{};
    vk::CommandBuffer m_command_buffer{};
};

constexpr unsigned int frame_overlap = 2;

struct ComputePushConstants {
    glm::vec4 m_data1;
    glm::vec4 m_data2;
    glm::vec4 m_data3;
    glm::vec4 m_data4;
};

struct ComputeEffect {
    const char* m_name;

    vk::Pipeline m_pipeline{};
    vk::PipelineLayout m_layout{};

    ComputePushConstants m_data;
};

struct MaterialConstants {
    glm::vec4 m_color_factors{1.0f};
    glm::vec4 m_metal_rough_factors{1.0f};
    glm::vec4 m_extra[14]{};
};

struct MaterialResources {
    const AllocatedImage* m_color_image{nullptr};
    const AllocatedSampler* m_color_sampler{nullptr};
    const AllocatedImage* m_metal_rough_image{nullptr};
    const AllocatedSampler* m_metal_rough_sampler{nullptr};
    const AllocatedBuffer* m_data_buffer{nullptr};
    uint32_t m_data_buffer_offset{0};
};

struct GltfMetallicRoughness {
    MaterialPipeline m_opaque_pipeline;
    MaterialPipeline m_transparent_pipeline;

    vk::DescriptorSetLayout m_material_layout{};

    void buildPipelines(VulkanEngine* engine);
    void clearResources(vk::Device device);

    MaterialInstance writeMaterial(vk::Device device,
                                    MaterialPass pass,
                                    const MaterialResources& resources,
                                    DescriptorAllocator& descriptor_allocator);
};

class VulkanEngine {
public:
    using RenderFunction = std::function<void(FrameRenderContext&)>;
    using OverlayRenderFunction =
        std::function<void(RenderCommandList&, const luna::vkcore::ImageView&, luna::render::Extent2D)>;
    using BeforePresentFunction = std::function<void()>;

    bool m_is_initialized{false};
    int m_frame_number{0};
    vk::Extent2D m_window_extent{1'700, 900};
    bool m_resize_requested{false};
    float m_render_scale{1.0f};

    GLFWwindow* m_window{nullptr};

    static VulkanEngine& get();

    bool init(luna::Window& window_ref);
    void cleanup();
    void draw(const RenderFunction& render_function = {},
              const OverlayRenderFunction& overlay_renderer = {},
              const BeforePresentFunction& before_present = {});
    void requestSwapchainResize()
    {
        m_resize_requested = true;
    }
    bool isSwapchainResizeRequested() const
    {
        return m_resize_requested;
    }
    bool resizeSwapchain();

    uint32_t getSwapchainImageCount() const
    {
        return m_swapchain_context.getImageCount();
    }

    luna::render::PixelFormat getSwapchainImageFormat() const
    {
        return fromVk(m_swapchain_context.getImageFormat());
    }

    luna::render::Extent2D getSwapchainExtent() const
    {
        return fromVk(m_swapchain_context.getExtent());
    }

    bool hasDevice() const
    {
        return m_device != VK_NULL_HANDLE;
    }

    vk::Instance getInstanceHandle() const
    {
        return m_instance;
    }

    vk::PhysicalDevice getPhysicalDeviceHandle() const
    {
        return m_chosen_gpu;
    }

    vk::Device getDeviceHandle() const
    {
        return m_device;
    }

    vk::Queue getGraphicsQueueHandle() const
    {
        return m_graphics_queue;
    }

    uint32_t getGraphicsQueueFamily() const
    {
        return m_graphics_queue_family;
    }

    const luna::vkcore::Instance& getInstanceContext() const
    {
        return m_instance_context;
    }

    const luna::vkcore::PhysicalDevice& getPhysicalDeviceContext() const
    {
        return m_physical_device_context;
    }

    const luna::vkcore::Device& getDeviceContext() const
    {
        return m_device_context;
    }

    const luna::vkcore::Queue& getGraphicsQueueContext() const
    {
        return m_graphics_queue_context;
    }

    const luna::vkcore::Swapchain& getSwapchainContext() const
    {
        return m_swapchain_context;
    }

    luna::vkcore::Swapchain& getSwapchainContext()
    {
        return m_swapchain_context;
    }

    const luna::vkcore::RenderTarget& getRenderTarget() const
    {
        return m_render_target;
    }

    luna::vkcore::RenderTarget& getRenderTarget()
    {
        return m_render_target;
    }

    vk::DeviceSize getUniformBufferAlignment() const
    {
        return m_uniform_buffer_alignment;
    }

    luna::vkcore::FrameContext m_frames[frame_overlap];
    ImmediateSubmitContext m_imm_context;

    luna::vkcore::FrameContext& getCurrentFrame()
    {
        return m_frames[m_frame_number % frame_overlap];
    }

    // temporary
    std::vector<ComputeEffect>& getBackgroundEffects()
    {
        return m_background_effects;
    }

    // temporary
    int& getCurrentBackgroundEffect()
    {
        return m_current_background_effect;
    }

    const int& getCurrentBackgroundEffect() const
    {
        return m_current_background_effect;
    }

    AllocatedImage createImage(luna::render::Extent3D size,
                                luna::render::PixelFormat format,
                                luna::render::ImageUsage usage,
                                bool mipmapped = false);
    AllocatedImage createImage(void* data,
                                luna::render::Extent3D size,
                                luna::render::PixelFormat format,
                                luna::render::ImageUsage usage,
                                bool mipmapped = false);
    AllocatedBuffer createBuffer(size_t alloc_size, luna::render::BufferUsage usage, luna::render::MemoryUsage memory_usage);
    luna::vkcore::ReleaseQueue m_shutdown_release;
    luna::vkcore::DeferredRelease m_deferred_release;

    luna::vkcore::VMAAllocator m_allocator;
    luna::vkcore::RenderTarget m_render_target;
    vk::Extent2D m_draw_extent{};

    DescriptorAllocator m_global_descriptor_allocator;

    vk::DescriptorSet m_draw_image_descriptors{};
    vk::DescriptorSetLayout m_draw_image_descriptor_layout{};
    vk::DescriptorSetLayout m_gpu_scene_data_descriptor_layout{};
    uint32_t m_draw_image_descriptor_binding{0};
    uint32_t m_draw_image_descriptor_count{1};
    vk::DescriptorType m_draw_image_descriptor_type{vk::DescriptorType::eStorageImage};

    AllocatedImage m_white_image;
    AllocatedImage m_black_image;
    AllocatedImage m_grey_image;
    AllocatedImage m_error_checkerboard_image;
    AllocatedSampler m_default_sampler_linear;
    AllocatedSampler m_default_sampler_nearest;

    vk::Pipeline m_gradient_pipeline{};
    vk::PipelineLayout m_gradient_pipeline_layout{};

    std::vector<ComputeEffect> m_background_effects;
    int m_current_background_effect{0};

    vk::PipelineLayout m_triangle_pipeline_layout{};
    vk::Pipeline m_triangle_pipeline{};
    MaterialInstance m_default_material_instance;
    GltfMetallicRoughness m_metal_rough_material;

    GPUMeshBuffers m_rectangle;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> m_loaded_scenes;
    DrawContext m_main_draw_context;
    GPUSceneData m_scene_data{};
    Camera m_main_camera;

    void destroyImage(AllocatedImage& image);
    void destroyBuffer(AllocatedBuffer& buffer);
    void deferRelease(std::function<void()>&& function);
    void immediateSubmit(const std::function<void(RenderCommandList&)>& function);
    void recordDefaultScene(FrameRenderContext& render_context);

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    friend class LoadedGLTF;
    friend std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine,
                                                               const std::filesystem::path& file_path);

private:
    luna::vkcore::Instance m_instance_context;
    luna::vkcore::PhysicalDevice m_physical_device_context;
    luna::vkcore::Device m_device_context;
    luna::vkcore::Queue m_graphics_queue_context;
    luna::vkcore::Swapchain m_swapchain_context;

    vk::Instance m_instance{};
    vk::DebugUtilsMessengerEXT m_debug_messenger{};
    vk::PhysicalDevice m_chosen_gpu{};
    vk::Device m_device{};
    vk::SurfaceKHR m_surface{};
    vk::Queue m_graphics_queue{};
    uint32_t m_graphics_queue_family{0};
    vk::DeviceSize m_uniform_buffer_alignment{1};

    bool initVulkan();
    bool initSwapchain();
    bool initCommands();
    bool initSyncStructures();
    bool initDescriptors();
    bool initPipelines();
    bool initBackgroundPipelines();
    void initTrianglePipeline();
    void initMeshPipeline();
    void initDefaultData();

    bool createSwapchain(uint32_t width, uint32_t height);
    bool createRenderTarget(vk::Extent2D extent);
    void destroyRenderTarget();

    AllocatedImage createImage(vk::Extent3D size,
                                vk::Format format,
                                vk::ImageUsageFlags usage,
                                bool mipmapped = false);
    AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false)
    {
        return createImage(toVk(size),
                            static_cast<vk::Format>(format),
                            static_cast<vk::ImageUsageFlags>(usage),
                            mipmapped);
    }
    AllocatedImage createImage(
        void* data, vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false)
    {
        return createImage(data,
                            toVk(size),
                            static_cast<vk::Format>(format),
                            static_cast<vk::ImageUsageFlags>(usage),
                            mipmapped);
    }
    AllocatedBuffer createBuffer(size_t alloc_size, vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage);
    AllocatedBuffer createBuffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage)
    {
        return createBuffer(alloc_size, static_cast<vk::BufferUsageFlags>(usage), memory_usage);
    }
    void immediateSubmitVk(const std::function<void(vk::CommandBuffer cmd)>& function);

    void updateDrawImageDescriptors();
    vk::Extent2D getFramebufferExtent() const;

    void drawBackground(vk::CommandBuffer cmd);
    void drawGeometry(vk::CommandBuffer cmd);
    void updateScene();

    void destroyImageImmediate(AllocatedImage& image);
    void destroyBufferImmediate(AllocatedBuffer& buffer);
    void destroySwapchain();
};

