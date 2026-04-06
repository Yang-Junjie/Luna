#pragma once

#include "RHIAdapter.h"
#include "RHIQueue.h"
#include "RHISwapchain.h"

#include <memory>
#include <vector>

namespace luna {

class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;

    virtual DeviceHandle getHandle() const = 0;
    virtual RHIBackend getBackend() const = 0;
    virtual RHICapabilities getCapabilities() const = 0;
    virtual RHIDeviceLimits getDeviceLimits() const = 0;
    virtual RHIFormatSupport queryFormatSupport(PixelFormat format) const = 0;
    virtual RHISwapchainState getSwapchainState() const = 0;
    virtual IRHISurface* getPrimarySurface() = 0;
    virtual const IRHISurface* getPrimarySurface() const = 0;
    virtual IRHISwapchain* getPrimarySwapchain() = 0;
    virtual const IRHISwapchain* getPrimarySwapchain() const = 0;
    virtual IRHICommandQueue* getCommandQueue(RHIQueueType queueType) = 0;
    virtual const IRHICommandQueue* getCommandQueue(RHIQueueType queueType) const = 0;
    virtual RHIResult init(const DeviceCreateInfo& createInfo) = 0;
    virtual RHIResult waitIdle() = 0;
    virtual void shutdown() = 0;
    virtual RHIResult createCommandList(RHIQueueType queueType, std::unique_ptr<IRHICommandList>* outCommandList) = 0;
    virtual RHIResult createFence(std::unique_ptr<IRHIFence>* outFence, bool signaled = false) = 0;
    virtual RHIResult createBuffer(const BufferDesc& desc, BufferHandle* outHandle, const void* initialData = nullptr) = 0;
    virtual void destroyBuffer(BufferHandle handle) = 0;
    virtual RHIResult writeBuffer(BufferHandle handle,
                                  const void* data,
                                  uint64_t size,
                                  uint64_t offset = 0) = 0;
    virtual RHIResult readBuffer(BufferHandle handle, void* outData, uint64_t size, uint64_t offset = 0) = 0;
    virtual RHIResult createImage(const ImageDesc& desc, ImageHandle* outHandle, const void* initialData = nullptr) = 0;
    virtual void destroyImage(ImageHandle handle) = 0;
    virtual RHIResult createImageView(const ImageViewDesc& desc, ImageViewHandle* outHandle) = 0;
    virtual void destroyImageView(ImageViewHandle handle) = 0;
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
std::vector<std::unique_ptr<IRHIAdapter>> EnumerateRHIAdapters(RHIBackend backend);

} // namespace luna
