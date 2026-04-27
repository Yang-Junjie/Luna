#include "Buffer.h"
#include "Impls/Vulkan/VKAccelerationStructure.h"
#include "Impls/Vulkan/VKAdapter.h"
#include "Impls/Vulkan/VKBuffer.h"
#include "Impls/Vulkan/VKCommandBufferEncoder.h"
#include "Impls/Vulkan/VKDescriptorPool.h"
#include "Impls/Vulkan/VKDescriptorSetLayout.h"
#include "Impls/Vulkan/VKInstance.h"
#include "Impls/Vulkan/VKPipeline.h"
#include "Impls/Vulkan/VKPipelineLayout.h"
#include "Impls/Vulkan/VKQueryPool.h"
#include "Impls/Vulkan/VKQueue.h"
#include "Impls/Vulkan/VKRayTracingPipeline.h"
#include "Impls/Vulkan/VKSampler.h"
#include "Impls/Vulkan/VKShaderBindingTable.h"
#include "Impls/Vulkan/VKShaderModule.h"
#include "Impls/Vulkan/VKSurface.h"
#include "Impls/Vulkan/VKSwapchain.h"
#include "Impls/Vulkan/VKSynchronization.h"
#include "Impls/Vulkan/VKTexture.h"

#include <cstring>

#include <algorithm>
#include <Impls/Vulkan/VKDevice.h>

namespace luna::RHI {
Ref<VKDevice> VKDevice::Create(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo)
{
    auto device = CreateRef<VKDevice>(adapter, createInfo);
    return device;
}

VKDevice::VKDevice(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo)
{
    if (!adapter) {
        throw std::runtime_error("VKDevice created with null adapter");
    }
    m_parentAdapter = adapter;
    auto pyDevice = std::dynamic_pointer_cast<VKAdapter>(adapter)->GetPhysicalDevice();
    int presentQueueFamily = -1;
    if (createInfo.CompatibleSurface) {
        presentQueueFamily =
            std::dynamic_pointer_cast<VKSurface>(createInfo.CompatibleSurface)->GetPresentQueueFamilyIndex(adapter);
    }
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::vector<std::vector<float>> prioritiesStorage;
    prioritiesStorage.reserve(createInfo.QueueRequests.size());
    for (const auto& queueRequest : createInfo.QueueRequests) {
        uint32_t queueFamilyIndex =
            std::dynamic_pointer_cast<VKAdapter>(adapter)->FindQueueFamilyIndex(queueRequest.Type);
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = queueRequest.Count;
        prioritiesStorage.push_back(std::vector<float>(queueRequest.Count, queueRequest.Priority));
        queueCreateInfo.pQueuePriorities = prioritiesStorage.back().data();
        queueCreateInfos.push_back(queueCreateInfo);
        m_queueFamilyIndices.push_back(queueFamilyIndex);
    }
    vk::PhysicalDeviceFeatures features10{};
    vk::PhysicalDeviceVulkan11Features features11{};
    features11.shaderDrawParameters = VK_TRUE;
    vk::PhysicalDeviceVulkan12Features features12{};
    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = VK_TRUE;
    vk::PhysicalDeviceMeshShaderFeaturesEXT meshFeatures{};
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    vk::PhysicalDeviceFragmentShadingRateFeaturesKHR vrsFeatures{};
    std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                           VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                                           VK_KHR_MAINTENANCE2_EXTENSION_NAME,
                                           VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};
    for (const auto& feature : createInfo.EnabledFeatures) {
        switch (feature) {
            case DeviceFeature::GeometryShader:
                features10.geometryShader = VK_TRUE;
                break;
            case DeviceFeature::TessellationShader:
                features10.tessellationShader = VK_TRUE;
                break;
            case DeviceFeature::IndependentBlending:
                features10.independentBlend = VK_TRUE;
                break;
            case DeviceFeature::MultiDrawIndirect:
                features10.multiDrawIndirect = VK_TRUE;
                break;
            case DeviceFeature::FillModeNonSolid:
                features10.fillModeNonSolid = VK_TRUE;
                break;
            case DeviceFeature::WideLines:
                features10.wideLines = VK_TRUE;
                break;
            case DeviceFeature::SamplerAnisotropy:
                features10.samplerAnisotropy = VK_TRUE;
                break;
            case DeviceFeature::BindlessDescriptors:
                features12.descriptorIndexing = VK_TRUE;
                features12.runtimeDescriptorArray = VK_TRUE;
                features12.descriptorBindingPartiallyBound = VK_TRUE;
                features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
                features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
                features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
                break;
            case DeviceFeature::BufferDeviceAddress:
                features12.bufferDeviceAddress = VK_TRUE;
                extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                break;
            case DeviceFeature::TextureCompressionBC:
                features10.textureCompressionBC = VK_TRUE;
                break;
            case DeviceFeature::TextureCompressionASTC:
                features10.textureCompressionASTC_LDR = VK_TRUE;
                break;
            case DeviceFeature::MeshShader:
                meshFeatures.meshShader = VK_TRUE;
                meshFeatures.taskShader = VK_TRUE;
                extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
                break;
            case DeviceFeature::RayTracingPipeline:
                rtPipelineFeatures.rayTracingPipeline = VK_TRUE;
                extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
                features12.bufferDeviceAddress = VK_TRUE;
                extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                break;
            case DeviceFeature::AccelerationStructure:
                asFeatures.accelerationStructure = VK_TRUE;
                extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
                features12.bufferDeviceAddress = VK_TRUE;
                extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                break;
            case DeviceFeature::VariableRateShading:
                vrsFeatures.pipelineFragmentShadingRate = VK_TRUE;
                extensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
                break;
            case DeviceFeature::ShaderFloat64:
                features10.shaderFloat64 = VK_TRUE;
                break;
            case DeviceFeature::ShaderInt16:
                features10.shaderInt16 = VK_TRUE;
                break;
            default:
                break;
        }
    }
    std::sort(extensions.begin(), extensions.end(), [](const char* a, const char* b) {
        return strcmp(a, b) < 0;
    });
    extensions.erase(std::unique(extensions.begin(),
                                 extensions.end(),
                                 [](const char* a, const char* b) {
                                     return strcmp(a, b) == 0;
                                 }),
                     extensions.end());

    vk::PhysicalDeviceFeatures2 features2{};
    features2.features = features10;

    void* pNextChain = nullptr;
    auto ChainStruct = [&](auto& structure) {
        structure.pNext = pNextChain;
        pNextChain = &structure;
    };
    ChainStruct(features11);
    ChainStruct(features12);
    ChainStruct(features13);
    ChainStruct(meshFeatures);
    ChainStruct(rtPipelineFeatures);
    ChainStruct(asFeatures);
    ChainStruct(vrsFeatures);
    ChainStruct(features2);
    printf("VK Device: extensions=%zu, asFeature=%d, rtFeature=%d, bufAddr=%d\n",
           extensions.size(),
           (int) asFeatures.accelerationStructure,
           (int) rtPipelineFeatures.rayTracingPipeline,
           (int) features12.bufferDeviceAddress);
    for (auto e : extensions) {
        printf("  ext: %s\n", e);
    }

    vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo()
                                                .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
                                                .setPpEnabledExtensionNames(extensions.data())
                                                .setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()))
                                                .setPQueueCreateInfos(queueCreateInfos.data())
                                                .setPEnabledFeatures(nullptr)
                                                .setPNext(pNextChain);
    m_Device = pyDevice.createDevice(deviceCreateInfo);
    if (!m_Device) {
        throw std::runtime_error("Failed to create Vulkan logical device");
    }
    m_graphicsQueueFamilyIndex =
        std::dynamic_pointer_cast<VKAdapter>(adapter)->FindQueueFamilyIndex(QueueType::Graphics);
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = std::dynamic_pointer_cast<VKAdapter>(m_parentAdapter)->GetPhysicalDevice();
    allocatorInfo.device = m_Device;
    allocatorInfo.instance = std::dynamic_pointer_cast<VKAdapter>(m_parentAdapter)->GetInstance()->GetVulkanInstance();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    if (features12.bufferDeviceAddress) {
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }
    if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

VKDevice::~VKDevice()
{
    m_Device.waitIdle();
    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = nullptr;
    }
    for (auto& [threadId, poolData] : m_threadCommandPools) {
        while (!poolData.primaryBuffers.empty()) {
            poolData.primaryBuffers.pop();
        }
        while (!poolData.secondaryBuffers.empty()) {
            poolData.secondaryBuffers.pop();
        }
        if (poolData.pool) {
            m_Device.destroyCommandPool(poolData.pool);
        }
    }
    m_threadCommandPools.clear();
    m_Device.destroy();
}

Ref<Queue> VKDevice::GetQueue(QueueType type, uint32_t index)
{
    uint32_t familyIndex = m_parentAdapter->FindQueueFamilyIndex(type);
    vk::Queue vkQueue = m_Device.getQueue(familyIndex, index);
    return VKQueue::Create(shared_from_this(), vkQueue, type, index);
}

Ref<Swapchain> VKDevice::CreateSwapchain(const SwapchainCreateInfo& createInfo)
{
    return VKSwapchain::Create(shared_from_this(), createInfo);
}

std::vector<uint32_t> VKDevice::GetAllQueueFamilyIndices() const
{
    return m_queueFamilyIndices;
}

Ref<Adapter> VKDevice::GetParentAdapter() const
{
    return m_parentAdapter;
}

ThreadCommandPoolData& VKDevice::GetThreadCommandPool()
{
    std::thread::id thisThread = std::this_thread::get_id();
    {
        std::lock_guard<std::mutex> lock(m_commandPoolMutex);
        auto it = m_threadCommandPools.find(thisThread);
        if (it != m_threadCommandPools.end()) {
            return it->second;
        }
    }
    vk::CommandPoolCreateInfo poolInfo = vk::CommandPoolCreateInfo()
                                             .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
                                             .setQueueFamilyIndex(m_graphicsQueueFamilyIndex);
    ThreadCommandPoolData newPoolData;
    newPoolData.pool = m_Device.createCommandPool(poolInfo);
    {
        std::lock_guard<std::mutex> lock(m_commandPoolMutex);
        auto [it, inserted] = m_threadCommandPools.emplace(thisThread, std::move(newPoolData));
        return it->second;
    }
}

Ref<CommandBufferEncoder> VKDevice::CreateCommandBufferEncoder(CommandBufferType type)
{
    auto& poolData = GetThreadCommandPool();
    switch (type) {
        case CommandBufferType::Primary: {
            if (poolData.primaryBuffers.empty()) {
                vk::CommandBufferAllocateInfo allocateInfo = vk::CommandBufferAllocateInfo()
                                                                 .setCommandPool(poolData.pool)
                                                                 .setLevel(vk::CommandBufferLevel::ePrimary)
                                                                 .setCommandBufferCount(1);
                vk::CommandBuffer commandBuffer = m_Device.allocateCommandBuffers(allocateInfo).front();
                poolData.primaryBuffers.push(
                    VKCommandBufferEncoder::Create(shared_from_this(), commandBuffer, CommandBufferType::Primary));
            }
            auto commandBuffer = poolData.primaryBuffers.front();
            poolData.primaryBuffers.pop();
            return commandBuffer;
        }
        case CommandBufferType::Secondary: {
            if (poolData.secondaryBuffers.empty()) {
                vk::CommandBufferAllocateInfo allocateInfo = vk::CommandBufferAllocateInfo()
                                                                 .setCommandPool(poolData.pool)
                                                                 .setLevel(vk::CommandBufferLevel::eSecondary)
                                                                 .setCommandBufferCount(1);
                vk::CommandBuffer commandBuffer = m_Device.allocateCommandBuffers(allocateInfo).front();
                poolData.secondaryBuffers.push(
                    VKCommandBufferEncoder::Create(shared_from_this(), commandBuffer, CommandBufferType::Secondary));
            }
            auto commandBuffer = poolData.secondaryBuffers.front();
            poolData.secondaryBuffers.pop();
            return commandBuffer;
        }
        default:
            throw std::runtime_error("Unsupported CommandBufferType in CreateCommandBufferEncoder");
    }
}

void VKDevice::ResetCommandPool()
{
    auto& poolData = GetThreadCommandPool();
    m_Device.resetCommandPool(poolData.pool);
}

void VKDevice::ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder)
{
    auto& poolData = GetThreadCommandPool();
    auto vkEncoder = std::static_pointer_cast<VKCommandBufferEncoder>(encoder);
    if (encoder->GetCommandBufferType() == CommandBufferType::Primary) {
        poolData.primaryBuffers.push(vkEncoder);
    } else {
        poolData.secondaryBuffers.push(vkEncoder);
    }
}

void VKDevice::FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder)
{
    auto& poolData = GetThreadCommandPool();
    auto* cmdBuffer = static_cast<VKCommandBufferEncoder*>(encoder.get());
    m_Device.freeCommandBuffers(poolData.pool, cmdBuffer->GetHandle());
}

void VKDevice::ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder)
{
    auto* cmdBuffer = static_cast<VKCommandBufferEncoder*>(encoder.get());
    cmdBuffer->GetHandle().reset(vk::CommandBufferResetFlagBits::eReleaseResources);
}

Ref<Texture> VKDevice::CreateTexture(const TextureCreateInfo& createInfo)
{
    return VKTexture::Create(shared_from_this(), m_allocator, createInfo);
}

Ref<Buffer> VKDevice::CreateBuffer(const BufferCreateInfo& createInfo)
{
    return VKBuffer::Create(shared_from_this(), m_allocator, createInfo);
}

Ref<Sampler> VKDevice::CreateSampler(const SamplerCreateInfo& createInfo)
{
    return VKSampler::Create(shared_from_this(), createInfo);
}

std::shared_ptr<DescriptorSetLayout> VKDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
{
    return VKDescriptorSetLayout::Create(shared_from_this(), info);
}

std::shared_ptr<DescriptorPool> VKDevice::CreateDescriptorPool(const DescriptorPoolCreateInfo& info)
{
    return VKDescriptorPool::Create(shared_from_this(), info);
}

Ref<ShaderModule> VKDevice::CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
{
    return VKShaderModule::Create(shared_from_this(), info, blob);
}

Ref<PipelineLayout> VKDevice::CreatePipelineLayout(const PipelineLayoutCreateInfo& info)
{
    return VKPipelineLayout::Create(shared_from_this(), info);
}

Ref<PipelineCache> VKDevice::CreatePipelineCache(std::span<const uint8_t> initialData)
{
    return VKPipelineCache::Create(shared_from_this(), initialData);
}

Ref<GraphicsPipeline> VKDevice::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info)
{
    ValidateGraphicsPipeline(info);
    return VKGraphicsPipeline::Create(shared_from_this(), info);
}

Ref<ComputePipeline> VKDevice::CreateComputePipeline(const ComputePipelineCreateInfo& info)
{
    return VKComputePipeline::Create(shared_from_this(), info);
}

Ref<Synchronization> VKDevice::CreateSynchronization(uint32_t maxFramesInFlight)
{
    return VKSynchronization::Create(shared_from_this(), maxFramesInFlight);
}

Ref<QueryPool> VKDevice::CreateQueryPool(const QueryPoolCreateInfo& info)
{
    return std::make_shared<VKQueryPool>(std::static_pointer_cast<VKDevice>(shared_from_this()), info);
}

Ref<AccelerationStructure> VKDevice::CreateAccelerationStructure(const AccelerationStructureCreateInfo& info)
{
    return std::make_shared<VKAccelerationStructure>(shared_from_this(), info);
}

Ref<RayTracingPipeline> VKDevice::CreateRayTracingPipeline(const RayTracingPipelineCreateInfo& info)
{
    return std::make_shared<VKRayTracingPipeline>(shared_from_this(), info);
}

Ref<ShaderBindingTable> VKDevice::CreateShaderBindingTable(const Ref<RayTracingPipeline>& pipeline,
                                                           uint32_t rayGenCount,
                                                           uint32_t missCount,
                                                           uint32_t hitGroupCount,
                                                           uint32_t callableCount)
{
    auto* vkPipeline = static_cast<VKRayTracingPipeline*>(pipeline.get());
    return std::make_shared<VKShaderBindingTable>(
        shared_from_this(), vkPipeline->GetHandle(), rayGenCount, missCount, hitGroupCount, callableCount);
}
} // namespace luna::RHI
