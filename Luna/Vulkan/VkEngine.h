#pragma once

#include "Core/Window.h"
#include "Camera.h"
#include "VkDescriptors.h"
#include "VkLoader.h"
#include "VkTypes.h"

#include <unordered_map>

struct GLFWwindow;
class VulkanEngine;

struct DeletionQueue {
    std::deque<std::function<void()>> m_deletors;

    // TODO(Yang) : need to be optimalized, because the std::funcation is too slow
    void pushFunction(std::function<void()>&& function)
    {
        m_deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = m_deletors.rbegin(); it != m_deletors.rend(); it++) {
            (*it)(); // call functors
        }

        m_deletors.clear();
    }
};

struct FrameData {
    vk::Semaphore m_swapchain_semaphore{};
    vk::Semaphore m_render_semaphore{};
    vk::Fence m_render_fence{};

    vk::CommandPool m_command_pool{};
    vk::CommandBuffer m_main_command_buffer{};
    DescriptorAllocatorGrowable m_frame_descriptors;

    DeletionQueue m_deletion_queue;
};

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
    AllocatedImage m_color_image;
    vk::Sampler m_color_sampler{};
    AllocatedImage m_metal_rough_image;
    vk::Sampler m_metal_rough_sampler{};
    AllocatedBuffer m_data_buffer;
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
    using OverlayRenderFunction = std::function<void(vk::CommandBuffer, vk::ImageView, vk::Extent2D)>;
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
    void draw(const OverlayRenderFunction& overlay_renderer = {}, const BeforePresentFunction& before_present = {});
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
        return static_cast<uint32_t>(m_swapchain_images.size());
    }

    vk::Format getSwapchainImageFormat() const
    {
        return m_swapchain_image_format;
    }

    FrameData m_frames[frame_overlap];
    ImmediateSubmitContext m_imm_context;

    FrameData& getCurrentFrame()
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

    vk::Instance m_instance{};
    vk::DebugUtilsMessengerEXT m_debug_messenger{};
    vk::PhysicalDevice m_chosen_gpu{};
    vk::Device m_device{};
    vk::SurfaceKHR m_surface{};
    vk::Queue m_graphics_queue{};
    uint32_t m_graphics_queue_family{0};

    vk::SwapchainKHR m_swapchain{};
    vk::Format m_swapchain_image_format{vk::Format::eUndefined};

    std::vector<vk::Image> m_swapchain_images;
    std::vector<vk::ImageView> m_swapchain_image_views;
    std::vector<VkImageLayout> m_swapchain_image_layouts;
    vk::Extent2D m_swapchain_extent{};

    DeletionQueue m_main_deletion_queue;

    VmaAllocator m_allocator{VK_NULL_HANDLE};

    AllocatedImage m_draw_image;
    AllocatedImage m_depth_image;
    vk::Extent2D m_draw_extent{};
    VkImageLayout m_draw_image_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout m_depth_image_layout{VK_IMAGE_LAYOUT_UNDEFINED};

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
    vk::Sampler m_default_sampler_linear{};
    vk::Sampler m_default_sampler_nearest{};

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
    AllocatedImage createImage(
        void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false)
    {
        return createImage(data,
                            toVk(size),
                            static_cast<vk::Format>(format),
                            static_cast<vk::ImageUsageFlags>(usage),
                            mipmapped);
    }
    void destroyImage(const AllocatedImage& image);
    void destroyBuffer(const AllocatedBuffer& buffer);

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    friend class LoadedGLTF;
    friend std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine,
                                                               const std::filesystem::path& file_path);

private:
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
    bool createDrawResources(vk::Extent2D extent);
    void destroyDrawResources();

    AllocatedBuffer createBuffer(size_t alloc_size, vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage);
    AllocatedBuffer createBuffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage)
    {
        return createBuffer(alloc_size, static_cast<vk::BufferUsageFlags>(usage), memory_usage);
    }
    void immediateSubmit(const std::function<void(vk::CommandBuffer cmd)>& function);

    void updateDrawImageDescriptors();
    vk::Extent2D getFramebufferExtent() const;

    void drawBackground(vk::CommandBuffer cmd);
    void drawGeometry(vk::CommandBuffer cmd);
    void updateScene();

    void destroySwapchain();
};

