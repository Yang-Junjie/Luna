#pragma once

#include "RHI/RHIDevice.h"
#include "vk_descriptors.h"
#include "vk_engine.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace luna {

class VulkanShader;
class ImGuiLayer;

class VulkanRHIDevice final : public IRHIDevice {
public:
    VulkanRHIDevice();
    ~VulkanRHIDevice() override;

    RHIBackend getBackend() const override;
    RHICapabilities getCapabilities() const override;
    RHIResult init(const DeviceCreateInfo& createInfo) override;
    RHIResult waitIdle() override;
    void shutdown() override;

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

    VulkanEngine& getEngine()
    {
        return m_engine;
    }

    const VulkanEngine& getEngine() const
    {
        return m_engine;
    }

private:
    class CommandContext;

    struct BufferResource {
        BufferDesc desc{};
        AllocatedBuffer buffer{};
    };

    struct ImageResource {
        ImageDesc desc{};
        AllocatedImage image{};
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        bool owned = true;
    };

    struct ImageViewResource {
        ImageViewDesc desc{};
        ImageHandle imageHandle{};
        vk::ImageView view{};
    };

    struct SamplerResource {
        SamplerDesc desc{};
        vk::Sampler sampler{};
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
        vk::DescriptorSet set{};
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
    bool ensureFramebufferReady();
    uint64_t nextHandleValue(uint64_t* counter);

private:
    DeviceCreateInfo m_createInfo{};
    RHICapabilities m_capabilities = QueryRHICapabilities(RHIBackend::Vulkan);
    VulkanEngine m_engine;
    std::unique_ptr<CommandContext> m_commandContext;
    DescriptorAllocatorGrowable m_resourceSetAllocator;
    VulkanResourceBindingRegistry m_bindingRegistry;

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

    bool m_loggedBeginFramePass = false;
    bool m_loggedAcquirePass = false;
    bool m_loggedPresentPass = false;
};

} // namespace luna
