#include "Impls/WebGPU/WGPUDevice.h"
#include "Impls/WebGPU/WGPUAdapter.h"
#include "Impls/WebGPU/WGPUSwapchain.h"
#include "Impls/WebGPU/WGPUCommandBufferEncoder.h"
#include "Impls/WebGPU/WGPUTexture.h"
#include "Impls/WebGPU/WGPUBuffer.h"
#include "Impls/WebGPU/WGPUPipeline.h"
#include "Impls/WebGPU/WGPUDescriptorSet.h"
#include "Impls/WebGPU/WGPUQueue.h"
#include "Impls/WebGPU/WGPUCommon.h"
#include <iostream>

namespace Cacao
{
    WGPUDevice::WGPUDevice(Ref<Adapter> adapter, const DeviceCreateInfo& info)
        : m_parentAdapter(adapter)
    {
        auto wgpuAdapter = std::dynamic_pointer_cast<WGPUAdapter>(adapter);

        struct DeviceResult { ::WGPUDevice device = nullptr; bool done = false; };
        DeviceResult result;

        WGPUDeviceDescriptor desc = {};
        desc.label = "Cacao WGPUDevice";

        wgpuAdapterRequestDevice(wgpuAdapter->GetNativeAdapter(), &desc,
            [](WGPURequestDeviceStatus status, ::WGPUDevice device, const char* message, void* userdata)
            {
                auto* res = static_cast<DeviceResult*>(userdata);
                if (status == WGPURequestDeviceStatus_Success)
                    res->device = device;
                else
                    std::cerr << "WebGPU device request failed: " << (message ? message : "unknown") << std::endl;
                res->done = true;
            }, &result);

        m_device = result.device;
        if (!m_device)
            throw std::runtime_error("Failed to create WebGPU device");

        m_queue = wgpuDeviceGetQueue(m_device);

        wgpuDeviceSetUncapturedErrorCallback(m_device,
            [](WGPUErrorType type, const char* message, void*)
            {
                const char* typeStr = "Unknown";
                switch (type)
                {
                case WGPUErrorType_Validation:  typeStr = "Validation"; break;
                case WGPUErrorType_OutOfMemory: typeStr = "OutOfMemory"; break;
                case WGPUErrorType_Internal:    typeStr = "Internal"; break;
                default: break;
                }
                std::cerr << "[WebGPU " << typeStr << "] " << (message ? message : "") << std::endl;
            }, nullptr);
    }

    WGPUDevice::~WGPUDevice()
    {
        if (m_queue) wgpuQueueRelease(m_queue);
        if (m_device) wgpuDeviceRelease(m_device);
    }

    Ref<Queue> WGPUDevice::GetQueue(QueueType type, uint32_t index)
    {
        return std::make_shared<WGPUQueue>();
    }

    Ref<Swapchain> WGPUDevice::CreateSwapchain(const SwapchainCreateInfo& info)
    {
        return std::make_shared<WGPUSwapchain>(shared_from_this(), info);
    }

    std::vector<uint32_t> WGPUDevice::GetAllQueueFamilyIndices() const { return {0}; }
    Ref<Adapter> WGPUDevice::GetParentAdapter() const { return m_parentAdapter; }

    Ref<CommandBufferEncoder> WGPUDevice::CreateCommandBufferEncoder(CommandBufferType type)
    {
        return std::make_shared<WGPUCommandBufferEncoder>();
    }

    void WGPUDevice::ResetCommandPool() {}
    void WGPUDevice::ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder) {}
    void WGPUDevice::FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder) {}
    void WGPUDevice::ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder) {}

    Ref<Texture> WGPUDevice::CreateTexture(const TextureCreateInfo& info)
    {
        return std::make_shared<WGPUTexture>(info);
    }

    Ref<Buffer> WGPUDevice::CreateBuffer(const BufferCreateInfo& info)
    {
        return std::make_shared<WGPUBuffer>(info);
    }

    Ref<Sampler> WGPUDevice::CreateSampler(const SamplerCreateInfo& info)
    {
        return std::make_shared<WGPUSampler>(info);
    }

    Ref<DescriptorSetLayout> WGPUDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
    {
        return std::make_shared<WGPUDescriptorSetLayout>(info);
    }

    Ref<DescriptorPool> WGPUDevice::CreateDescriptorPool(const DescriptorPoolCreateInfo& info)
    {
        return std::make_shared<WGPUDescriptorPool>(info);
    }

    Ref<ShaderModule> WGPUDevice::CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
    {
        return std::make_shared<WGPUShaderModule>(blob, info);
    }

    Ref<PipelineLayout> WGPUDevice::CreatePipelineLayout(const PipelineLayoutCreateInfo& info)
    {
        return std::make_shared<WGPUPipelineLayout>(info);
    }

    Ref<PipelineCache> WGPUDevice::CreatePipelineCache(std::span<const uint8_t> initialData)
    {
        return std::make_shared<WGPUPipelineCache>();
    }

    Ref<GraphicsPipeline> WGPUDevice::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info)
    {
        return std::make_shared<WGPUGraphicsPipeline>(shared_from_this(), info);
    }

    Ref<ComputePipeline> WGPUDevice::CreateComputePipeline(const ComputePipelineCreateInfo& info)
    {
        return std::make_shared<WGPUComputePipelineImpl>(shared_from_this(), info);
    }

    Ref<Synchronization> WGPUDevice::CreateSynchronization(uint32_t maxFramesInFlight)
    {
        return std::make_shared<WGPUSynchronization>(maxFramesInFlight);
    }
}
