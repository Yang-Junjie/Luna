#pragma once

#include "Core/window.h"
#include "RHI/Descriptors.h"
#include "camera.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_types.h"

#include <unordered_map>

struct GLFWwindow;
class VulkanEngine;
namespace luna {
class ImGuiLayer;
}

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
    vk::Semaphore _swapchainSemaphore{};
    vk::Semaphore _renderSemaphore{};
    vk::Fence _renderFence{};

    vk::CommandPool _commandPool{};
    vk::CommandBuffer _mainCommandBuffer{};
    DescriptorAllocatorGrowable _frameDescriptors;

    DeletionQueue _deletionQueue;
};

struct ImmediateSubmitContext {
    vk::Fence _fence{};
    vk::CommandPool _commandPool{};
    vk::CommandBuffer _commandBuffer{};
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

    vk::Pipeline pipeline{};
    vk::PipelineLayout layout{};

    ComputePushConstants data;
};

struct TriangleVertex {
    float position[2]{};
    float color[3]{};
};

struct MaterialConstants {
    glm::vec4 colorFactors{1.0f};
    glm::vec4 metal_rough_factors{1.0f};
    glm::vec4 extra[14]{};
};

struct MaterialResources {
    AllocatedImage colorImage;
    vk::Sampler colorSampler{};
    AllocatedImage metalRoughImage;
    vk::Sampler metalRoughSampler{};
    AllocatedBuffer dataBuffer;
    uint32_t dataBufferOffset{0};
};

struct GLTFMetallic_Roughness {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    vk::DescriptorSetLayout materialLayout{};

    void build_pipelines(VulkanEngine* engine);
    void clear_resources(vk::Device device);

    MaterialInstance write_material(vk::Device device,
                                    MaterialPass pass,
                                    const MaterialResources& resources,
                                    DescriptorAllocator& descriptorAllocator);
};

class VulkanEngine {
public:
    using OverlayRenderFunction = std::function<void(vk::CommandBuffer, vk::ImageView, vk::Extent2D)>;
    using BeforePresentFunction = std::function<void()>;
    enum class LegacyRendererMode : uint8_t {
        LegacyScene = 0,
        ClearColor,
        Triangle,
        ComputeBackground
    };

    bool _isInitialized{false};
    int _frameNumber{0};
    vk::Extent2D _windowExtent{1'700, 900};
    bool resize_requested{false};
    float renderScale{1.0f};

    GLFWwindow* _window{nullptr};

    static VulkanEngine& Get();

    bool init(luna::Window& window);
    void cleanup();
    void draw(const OverlayRenderFunction& overlayRenderer = {}, const BeforePresentFunction& beforePresent = {});
    void drawImGui(luna::ImGuiLayer* imguiLayer);
    void setLegacyRendererMode(LegacyRendererMode mode)
    {
        m_legacyRendererMode = mode;
    }
    LegacyRendererMode getLegacyRendererMode() const
    {
        return m_legacyRendererMode;
    }
    void setDemoClearColor(vk::ClearColorValue clearColor)
    {
        m_demoClearColor = clearColor;
    }
    void setDemoClearColor(float r, float g, float b, float a)
    {
        m_demoClearColor.float32[0] = r;
        m_demoClearColor.float32[1] = g;
        m_demoClearColor.float32[2] = b;
        m_demoClearColor.float32[3] = a;
    }
    void setTriangleShaderPaths(std::filesystem::path vertexShaderPath, std::filesystem::path fragmentShaderPath)
    {
        m_triangleVertexShaderPath = std::move(vertexShaderPath);
        m_triangleFragmentShaderPath = std::move(fragmentShaderPath);
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

    uint32_t getSwapchainImageCount() const
    {
        return static_cast<uint32_t>(_swapchainImages.size());
    }

    vk::Format getSwapchainImageFormat() const
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

    vk::Instance _instance{};
    vk::DebugUtilsMessengerEXT _debug_messenger{};
    vk::PhysicalDevice _chosenGPU{};
    vk::Device _device{};
    vk::SurfaceKHR _surface{};
    vk::Queue _graphicsQueue{};
    uint32_t _graphicsQueueFamily{0};

    vk::SwapchainKHR _swapchain{};
    vk::Format _swapchainImageFormat{vk::Format::eUndefined};

    std::vector<vk::Image> _swapchainImages;
    std::vector<vk::ImageView> _swapchainImageViews;
    std::vector<VkImageLayout> _swapchainImageLayouts;
    vk::Extent2D _swapchainExtent{};

    DeletionQueue _mainDeletionQueue;

    VmaAllocator _allocator{VK_NULL_HANDLE};

    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    vk::Extent2D _drawExtent{};
    VkImageLayout _drawImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout _depthImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};

    DescriptorAllocator globalDescriptorAllocator;

    vk::DescriptorSet _drawImageDescriptors{};
    vk::DescriptorSetLayout _drawImageDescriptorLayout{};
    vk::DescriptorSetLayout _gpuSceneDataDescriptorLayout{};
    uint32_t _drawImageDescriptorBinding{0};
    uint32_t _drawImageDescriptorCount{1};
    vk::DescriptorType _drawImageDescriptorType{vk::DescriptorType::eStorageImage};

    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;
    vk::Sampler _defaultSamplerLinear{};
    vk::Sampler _defaultSamplerNearest{};

    vk::Pipeline _gradientPipeline{};
    vk::PipelineLayout _gradientPipelineLayout{};

    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect{0};

    vk::PipelineLayout _trianglePipelineLayout{};
    vk::Pipeline _trianglePipeline{};
    MaterialInstance _defaultMaterialInstance;
    GLTFMetallic_Roughness metalRoughMaterial;

    GPUMeshBuffers rectangle;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
    DrawContext mainDrawContext;
    GPUSceneData sceneData{};
    Camera mainCamera;

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
    vk::Sampler create_sampler(const luna::SamplerDesc& desc);
    void destroy_image(const AllocatedImage& image);
    void destroy_buffer(const AllocatedBuffer& buffer);
    bool uploadBufferData(const AllocatedBuffer& buffer, const void* data, size_t size, size_t offset = 0);
    bool uploadTriangleVertices(std::span<const TriangleVertex> vertices);
    uint32_t getTriangleVertexCount() const
    {
        return m_triangleVertexCount;
    }

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    friend class LoadedGLTF;
    friend std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine,
                                                               const std::filesystem::path& filePath);

private:
    bool uses_background_compute() const;
    bool uses_scene_renderer() const;
    void clear_draw_image(vk::CommandBuffer cmd);
    void record_draw_pass(vk::CommandBuffer cmd);

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
    bool create_draw_resources(vk::Extent2D extent);
    void destroy_draw_resources();

    AllocatedBuffer create_buffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
    {
        return create_buffer(allocSize, static_cast<vk::BufferUsageFlags>(usage), memoryUsage);
    }
    void immediate_submit(const std::function<void(vk::CommandBuffer cmd)>& function);

    void update_draw_image_descriptors();
    vk::Extent2D get_framebuffer_extent() const;

    void record_compute_background(vk::CommandBuffer cmd);
    void record_scene_geometry(vk::CommandBuffer cmd);
    void record_triangle_geometry(vk::CommandBuffer cmd);
    void update_scene();

    void destroy_swapchain();

private:
    LegacyRendererMode m_legacyRendererMode = LegacyRendererMode::LegacyScene;
    vk::ClearColorValue m_demoClearColor = [] {
        vk::ClearColorValue value{};
        value.float32[0] = 0.08f;
        value.float32[1] = 0.12f;
        value.float32[2] = 0.18f;
        value.float32[3] = 1.0f;
        return value;
    }();
    std::filesystem::path m_triangleVertexShaderPath;
    std::filesystem::path m_triangleFragmentShaderPath;
    AllocatedBuffer m_triangleVertexBuffer{};
    uint32_t m_triangleVertexCount = 0;
    bool m_loggedBeginFramePass = false;
    bool m_loggedPresentPass = false;
};
