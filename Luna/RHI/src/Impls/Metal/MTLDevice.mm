#ifdef __APPLE__

#include "Impls/Metal/MTLDevice.h"
#include "Impls/Metal/MTLAdapter.h"
#include "Impls/Metal/MTLSwapchain.h"
#include "Impls/Metal/MTLCommandBufferEncoder.h"
#include "Impls/Metal/MTLTexture.h"
#include "Impls/Metal/MTLBuffer.h"
#include "Impls/Metal/MTLPipeline.h"
#include "Impls/Metal/MTLDescriptorSet.h"
#include "Impls/Metal/MTLQueue.h"
#import <Metal/Metal.h>
#include <iostream>

namespace Cacao
{
    MTLDevice::MTLDevice(Ref<Adapter> adapter, const DeviceCreateInfo& info)
        : m_parentAdapter(adapter)
    {
        auto mtlAdapter = std::dynamic_pointer_cast<MTLAdapter>(adapter);
        m_device = mtlAdapter->GetNativeDevice();
        m_commandQueue = [m_device newCommandQueue];
        if (!m_commandQueue)
            throw std::runtime_error("Failed to create Metal command queue");
    }

    MTLDevice::~MTLDevice()
    {
        m_commandQueue = nil;
        m_device = nil;
    }

    Ref<Queue> MTLDevice::GetQueue(QueueType type, uint32_t index)
    {
        return std::make_shared<MTLQueue>(m_commandQueue);
    }

    Ref<Swapchain> MTLDevice::CreateSwapchain(const SwapchainCreateInfo& info)
    {
        return std::make_shared<MTLSwapchain>(shared_from_this(), info);
    }

    std::vector<uint32_t> MTLDevice::GetAllQueueFamilyIndices() const { return {0}; }
    Ref<Adapter> MTLDevice::GetParentAdapter() const { return m_parentAdapter; }

    Ref<CommandBufferEncoder> MTLDevice::CreateCommandBufferEncoder(CommandBufferType type)
    {
        return std::make_shared<MTLCommandBufferEncoder>(m_commandQueue);
    }

    void MTLDevice::ResetCommandPool() {}
    void MTLDevice::ReturnCommandBuffer(const Ref<CommandBufferEncoder>&) {}
    void MTLDevice::FreeCommandBuffer(const Ref<CommandBufferEncoder>&) {}
    void MTLDevice::ResetCommandBuffer(const Ref<CommandBufferEncoder>&) {}

    Ref<Texture> MTLDevice::CreateTexture(const TextureCreateInfo& info)
    {
        return std::make_shared<MTLTexture>(m_device, info);
    }

    Ref<Buffer> MTLDevice::CreateBuffer(const BufferCreateInfo& info)
    {
        return std::make_shared<MTLBuffer>(m_device, info);
    }

    Ref<Sampler> MTLDevice::CreateSampler(const SamplerCreateInfo& info)
    {
        return std::make_shared<MTLSamplerObj>(m_device, info);
    }

    Ref<DescriptorSetLayout> MTLDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
    {
        return std::make_shared<MTLDescriptorSetLayout>(info);
    }

    Ref<DescriptorPool> MTLDevice::CreateDescriptorPool(const DescriptorPoolCreateInfo& info)
    {
        return std::make_shared<MTLDescriptorPool>(info);
    }

    Ref<ShaderModule> MTLDevice::CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
    {
        return std::make_shared<MTLShaderModule>(m_device, blob, info);
    }

    Ref<PipelineLayout> MTLDevice::CreatePipelineLayout(const PipelineLayoutCreateInfo& info)
    {
        return std::make_shared<MTLPipelineLayout>(info);
    }

    Ref<PipelineCache> MTLDevice::CreatePipelineCache(std::span<const uint8_t> initialData)
    {
        return std::make_shared<MTLPipelineCacheImpl>();
    }

    Ref<GraphicsPipeline> MTLDevice::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info)
    {
        return std::make_shared<MTLGraphicsPipeline>(shared_from_this(), info);
    }

    Ref<ComputePipeline> MTLDevice::CreateComputePipeline(const ComputePipelineCreateInfo& info)
    {
        return std::make_shared<MTLComputePipeline>(shared_from_this(), info);
    }

    Ref<Synchronization> MTLDevice::CreateSynchronization(uint32_t maxFramesInFlight)
    {
        return std::make_shared<MTLSynchronization>(maxFramesInFlight);
    }
}

#endif // __APPLE__
