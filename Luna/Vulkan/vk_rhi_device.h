#pragma once

#include "RHI/RHIDevice.h"
#include "vk_descriptors.h"
#include "vk_device_context.h"

#include <memory>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace luna {

class VulkanShader;
class ImGuiLayer;

class VulkanRHIDevice final : public IRHIDevice {
public:
    struct TimelineEvent {
        uint64_t serial = 0;
        std::string label;
    };

    struct DebugImageInfo {
        ImageDesc desc{};
        vk::ImageType backendImageType = vk::ImageType::e2D;
        vk::ImageViewType backendDefaultViewType = vk::ImageViewType::e2D;
        vk::ImageCreateFlags backendCreateFlags{};
        uint32_t backendLayerCount = 1;
    };

    struct DebugImageViewInfo {
        ImageViewDesc desc{};
        ImageHandle imageHandle{};
        vk::ImageViewType backendViewType = vk::ImageViewType::e2D;
        vk::Format backendFormat = vk::Format::eUndefined;
    };

    struct DebugSamplerInfo {
        SamplerDesc desc{};
        vk::Filter magFilter = vk::Filter::eLinear;
        vk::Filter minFilter = vk::Filter::eLinear;
        vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear;
        vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeW = vk::SamplerAddressMode::eRepeat;
        float mipLodBias = 0.0f;
        float minLod = 0.0f;
        float maxLod = 0.0f;
        bool anisotropyEnable = false;
        float maxAnisotropy = 1.0f;
        bool compareEnable = false;
        vk::CompareOp compareOp = vk::CompareOp::eLessOrEqual;
        vk::BorderColor borderColor = vk::BorderColor::eFloatTransparentBlack;
    };

    VulkanRHIDevice();
    ~VulkanRHIDevice() override;

    DeviceHandle getHandle() const override;
    RHIBackend getBackend() const override;
    RHICapabilities getCapabilities() const override;
    RHIDeviceLimits getDeviceLimits() const override;
    RHIFormatSupport queryFormatSupport(PixelFormat format) const override;
    RHISwapchainState getSwapchainState() const override;
    IRHISurface* getPrimarySurface() override;
    const IRHISurface* getPrimarySurface() const override;
    IRHISwapchain* getPrimarySwapchain() override;
    const IRHISwapchain* getPrimarySwapchain() const override;
    IRHICommandQueue* getCommandQueue(RHIQueueType queueType) override;
    const IRHICommandQueue* getCommandQueue(RHIQueueType queueType) const override;
    RHIResult init(const DeviceCreateInfo& createInfo) override;
    RHIResult waitIdle() override;
    void shutdown() override;
    RHIResult createCommandList(RHIQueueType queueType, std::unique_ptr<IRHICommandList>* outCommandList) override;
    RHIResult createFence(std::unique_ptr<IRHIFence>* outFence, bool signaled = false) override;

    RHIResult createBuffer(const BufferDesc& desc, BufferHandle* outHandle, const void* initialData = nullptr) override;
    void destroyBuffer(BufferHandle handle) override;
    RHIResult writeBuffer(BufferHandle handle, const void* data, uint64_t size, uint64_t offset = 0) override;
    RHIResult readBuffer(BufferHandle handle, void* outData, uint64_t size, uint64_t offset = 0) override;

    RHIResult createImage(const ImageDesc& desc, ImageHandle* outHandle, const void* initialData = nullptr) override;
    void destroyImage(ImageHandle handle) override;
    RHIResult createImageView(const ImageViewDesc& desc, ImageViewHandle* outHandle) override;
    void destroyImageView(ImageViewHandle handle) override;

    RHIResult createSampler(const SamplerDesc& desc, SamplerHandle* outHandle) override;
    void destroySampler(SamplerHandle handle) override;

    RHIResult createShader(const ShaderDesc& desc, ShaderHandle* outHandle) override;
    void destroyShader(ShaderHandle handle) override;
    const Shader::ReflectionMap* getShaderReflection(ShaderHandle handle) const override;

    RHIResult createResourceLayout(const ResourceLayoutDesc& desc, ResourceLayoutHandle* outHandle) override;
    void destroyResourceLayout(ResourceLayoutHandle handle) override;

    RHIResult createResourceSet(ResourceLayoutHandle layout, ResourceSetHandle* outHandle) override;
    RHIResult updateResourceSet(ResourceSetHandle resourceSet, const ResourceSetWriteDesc& writeDesc) override;
    void destroyResourceSet(ResourceSetHandle handle) override;

    RHIResult createGraphicsPipeline(const GraphicsPipelineDesc& desc, PipelineHandle* outHandle) override;
    RHIResult createComputePipeline(const ComputePipelineDesc& desc, PipelineHandle* outHandle) override;
    void destroyPipeline(PipelineHandle handle) override;

    RHIResult beginFrame(FrameContext* outFrameContext) override;
    RHIResult endFrame() override;
    RHIResult present() override;
    RHIResult renderOverlay(ImGuiLayer& imguiLayer);

    VulkanDeviceContext& getDeviceContext()
    {
        return m_context;
    }

    const VulkanDeviceContext& getDeviceContext() const
    {
        return m_context;
    }

    const std::vector<TimelineEvent>& getTimelineEvents() const
    {
        return m_timelineEvents;
    }

    bool debugGetImageInfo(ImageHandle handle, DebugImageInfo* outInfo) const;
    bool debugGetImageViewInfo(ImageViewHandle handle, DebugImageViewInfo* outInfo) const;
    bool debugGetSamplerInfo(SamplerHandle handle, DebugSamplerInfo* outInfo) const;
    float debugGetMaxSamplerAnisotropy() const;
    bool debugSupportsSamplerAnisotropy() const;

private:
    class CommandContext;
    class Surface;
    class Swapchain;
    class CommandQueue;
    class CommandList;
    class Fence;

    struct BufferResource {
        BufferDesc desc{};
        AllocatedBuffer buffer{};
    };

    struct ImageResource {
        ImageDesc desc{};
        AllocatedImage image{};
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        std::vector<vk::ImageLayout> subresourceLayouts;
        bool owned = true;
        vk::ImageType backendImageType = vk::ImageType::e2D;
        vk::ImageViewType backendDefaultViewType = vk::ImageViewType::e2D;
        vk::ImageCreateFlags backendCreateFlags{};
        uint32_t backendLayerCount = 1;
    };

    struct ImageViewResource {
        ImageViewDesc desc{};
        ImageHandle imageHandle{};
        vk::ImageView view{};
        vk::ImageViewType backendViewType = vk::ImageViewType::e2D;
        vk::Format backendFormat = vk::Format::eUndefined;
    };

    struct SamplerResource {
        SamplerDesc desc{};
        vk::Sampler sampler{};
        DebugSamplerInfo debugInfo{};
    };

    struct ShaderResource {
        ShaderDesc desc{};
        std::string filePath;
        std::vector<uint32_t> code;
        std::unique_ptr<VulkanShader> reflection;
    };

    struct ResourceLayoutResource {
        ResourceLayoutDesc desc{};
        vk::DescriptorSetLayout layout{};
    };

    struct ResourceSetResource {
        ResourceLayoutHandle layoutHandle{};
        vk::DescriptorPool pool{};
        vk::DescriptorSet set{};
    };

    struct RetirementEntry {
        uint64_t serial = 0;
        std::string label;
        std::function<void()> action;
    };

    struct PipelineResource {
        std::vector<ResourceLayoutHandle> resourceLayouts;
        uint32_t pushConstantSize = 0;
        vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics;
        vk::Pipeline pipeline{};
        vk::PipelineLayout layout{};
    };

    friend class CommandContext;

    BufferResource* findBuffer(BufferHandle handle);
    const BufferResource* findBuffer(BufferHandle handle) const;

    ImageResource* findImage(ImageHandle handle);
    const ImageResource* findImage(ImageHandle handle) const;

    ImageViewResource* findImageView(ImageViewHandle handle);
    const ImageViewResource* findImageView(ImageViewHandle handle) const;

    SamplerResource* findSampler(SamplerHandle handle);
    const SamplerResource* findSampler(SamplerHandle handle) const;

    ShaderResource* findShader(ShaderHandle handle);
    const ShaderResource* findShader(ShaderHandle handle) const;

    ResourceLayoutResource* findResourceLayout(ResourceLayoutHandle handle);
    const ResourceLayoutResource* findResourceLayout(ResourceLayoutHandle handle) const;

    ResourceSetResource* findResourceSet(ResourceSetHandle handle);
    const ResourceSetResource* findResourceSet(ResourceSetHandle handle) const;

    PipelineResource* findPipeline(PipelineHandle handle);
    const PipelineResource* findPipeline(PipelineHandle handle) const;

    void destroyAllResources();
    void refreshBackbufferHandle();
    void refreshRuntimeProperties();
    bool ensureFramebufferReady();
    RHIResult requestPrimarySwapchainRecreate();
    uint32_t imageLayerCount(const ImageDesc& desc) const;
    bool validateImageRange(const ImageResource& image,
                            uint32_t baseMipLevel,
                            uint32_t mipCount,
                            uint32_t baseArrayLayer,
                            uint32_t layerCount) const;
    vk::ImageLayout getImageSubresourceLayout(const ImageResource& image, uint32_t mipLevel, uint32_t arrayLayer) const;
    void setImageSubresourceLayout(ImageResource& image, uint32_t mipLevel, uint32_t arrayLayer, vk::ImageLayout layout);
    bool getImageRangeLayout(const ImageResource& image,
                             uint32_t baseMipLevel,
                             uint32_t mipCount,
                             uint32_t baseArrayLayer,
                             uint32_t layerCount,
                             vk::ImageLayout* outLayout) const;
    void setImageRangeLayout(ImageResource& image,
                             uint32_t baseMipLevel,
                             uint32_t mipCount,
                             uint32_t baseArrayLayer,
                             uint32_t layerCount,
                             vk::ImageLayout layout);
    uint64_t nextHandleValue(uint64_t* counter);
    uint64_t pendingSubmitSerial() const;
    void appendTimelineEvent(std::string label);
    void scheduleRetirement(uint64_t serial, std::string label, std::function<void()> action);
    void retireCompletedSerial(uint64_t serial);

private:
    DeviceCreateInfo m_createInfo{};
    RHICapabilities m_capabilities = QueryRHICapabilities(RHIBackend::Vulkan);
    RHIDeviceLimits m_limits{};
    RHISwapchainState m_swapchainState{};
    VulkanDeviceContext m_context;
    std::unique_ptr<CommandContext> m_commandContext;
    DescriptorAllocatorGrowable m_resourceSetAllocator;
    VulkanResourceBindingRegistry m_bindingRegistry;

    DeviceHandle m_deviceHandle{};
    GLFWwindow* m_nativeWindow = nullptr;
    std::string m_applicationName;
    bool m_initialized = false;
    bool m_frameInProgress = false;
    bool m_pendingPresent = false;
    bool m_rendering = false;
    bool m_recreateAfterPresent = false;
    bool m_resourceSetAllocatorInitialized = false;
    uint32_t m_swapchainImageIndex = 0;
    ImageHandle m_currentBackbufferHandle{};

    uint64_t m_nextShaderId = 1;
    uint64_t m_nextResourceLayoutId = 1;
    uint64_t m_nextResourceSetId = 1;
    uint64_t m_nextPipelineId = 1;
    uint64_t m_nextImageId = 1;
    uint64_t m_nextImageViewId = 1ull << 32;

    std::unordered_map<uint64_t, BufferResource> m_buffers;
    std::unordered_map<uint64_t, ImageResource> m_images;
    std::unordered_map<uint64_t, ImageViewResource> m_imageViews;
    std::unordered_map<uint64_t, SamplerResource> m_samplers;
    std::unordered_map<uint64_t, ShaderResource> m_shaders;
    std::unordered_map<uint64_t, ResourceLayoutResource> m_resourceLayouts;
    std::unordered_map<uint64_t, ResourceSetResource> m_resourceSets;
    std::unordered_map<uint64_t, PipelineResource> m_pipelines;
    std::deque<RetirementEntry> m_retirementQueue;
    std::vector<TimelineEvent> m_timelineEvents;
    uint64_t m_nextTimelineEventSerial = 0;
    uint64_t m_lastSubmittedSerial = 0;
    uint64_t m_lastCompletedSerial = 0;
    uint64_t m_descriptorRetireCount = 0;
    uint64_t m_descriptorRecycleCount = 0;

    bool m_loggedBeginFramePass = false;
    bool m_loggedAcquirePass = false;
    bool m_loggedPresentPass = false;
    uint64_t m_lastObservedSwapchainGeneration = 0;
    std::unique_ptr<Surface> m_primarySurface;
    std::unique_ptr<Swapchain> m_primarySwapchain;
    std::unique_ptr<CommandQueue> m_graphicsQueue;
};

std::vector<std::unique_ptr<IRHIAdapter>> EnumerateVulkanAdapters();

} // namespace luna
