#pragma once

#include "Descriptors.h"
#include "ResourceLayout.h"
#include "Shader.h"

#include <memory>
#include <string_view>

namespace luna {

class IRHICommandContext;

struct DeviceCreateInfo {
    std::string_view applicationName = "Luna";
    RHIBackend backend = RHIBackend::Vulkan;
    void* nativeWindow = nullptr;
    SwapchainDesc swapchain;
    bool enableValidation = false;
};

struct RHICapabilities {
    RHIBackend backend = RHIBackend::Vulkan;
    bool implemented = false;
    bool supportsGraphics = false;
    bool supportsPresent = false;
    bool supportsDynamicRendering = false;
    bool supportsIndexedDraw = false;
    bool supportsResourceSets = false;
    uint32_t framesInFlight = 0;
};

struct FrameContext {
    uint32_t frameIndex = 0;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    ImageHandle backbuffer{};
    PixelFormat backbufferFormat = PixelFormat::Undefined;
    IRHICommandContext* commandContext = nullptr;
};

class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;

    virtual RHIBackend getBackend() const = 0;
    virtual RHICapabilities getCapabilities() const = 0;
    virtual RHIResult init(const DeviceCreateInfo& createInfo) = 0;
    virtual RHIResult waitIdle() = 0;
    virtual void shutdown() = 0;
    virtual RHIResult createBuffer(const BufferDesc& desc, BufferHandle* outHandle, const void* initialData = nullptr) = 0;
    virtual void destroyBuffer(BufferHandle handle) = 0;
    virtual RHIResult writeBuffer(BufferHandle handle,
                                  const void* data,
                                  uint64_t size,
                                  uint64_t offset = 0) = 0;
    virtual RHIResult createImage(const ImageDesc& desc, ImageHandle* outHandle, const void* initialData = nullptr) = 0;
    virtual void destroyImage(ImageHandle handle) = 0;
    virtual RHIResult createSampler(const SamplerDesc& desc, SamplerHandle* outHandle) = 0;
    virtual void destroySampler(SamplerHandle handle) = 0;
    virtual RHIResult createShader(const ShaderDesc& desc, ShaderHandle* outHandle) = 0;
    virtual void destroyShader(ShaderHandle handle) = 0;
    virtual const Shader::ReflectionMap* getShaderReflection(ShaderHandle handle) const = 0;
    virtual RHIResult createResourceLayout(const ResourceLayoutDesc& desc, ResourceLayoutHandle* outHandle) = 0;
    virtual void destroyResourceLayout(ResourceLayoutHandle handle) = 0;
    virtual RHIResult createResourceSet(ResourceLayoutHandle layout, ResourceSetHandle* outHandle) = 0;
    virtual RHIResult updateResourceSet(ResourceSetHandle resourceSet, const ResourceSetWriteDesc& writeDesc) = 0;
    virtual void destroyResourceSet(ResourceSetHandle handle) = 0;
    virtual RHIResult createGraphicsPipeline(const GraphicsPipelineDesc& desc, PipelineHandle* outHandle) = 0;
    virtual RHIResult createComputePipeline(const ComputePipelineDesc& desc, PipelineHandle* outHandle) = 0;
    virtual void destroyPipeline(PipelineHandle handle) = 0;
    virtual RHIResult beginFrame(FrameContext* outFrameContext) = 0;
    virtual RHIResult endFrame() = 0;
    virtual RHIResult present() = 0;
};

RHICapabilities QueryRHICapabilities(RHIBackend backend) noexcept;
bool IsBackendImplemented(RHIBackend backend) noexcept;
std::unique_ptr<IRHIDevice> CreateRHIDevice(RHIBackend backend);

} // namespace luna
