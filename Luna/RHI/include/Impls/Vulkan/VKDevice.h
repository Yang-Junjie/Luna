#ifndef LUNA_RHI_VKDEVICE_H
#define LUNA_RHI_VKDEVICE_H
#include "Device.h"
#include "vk_mem_alloc.h"

#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class Adapter;
class VKCommandBufferEncoder;

struct ThreadCommandPoolData {
    vk::CommandPool pool;
    std::queue<Ref<VKCommandBufferEncoder>> primaryBuffers;
    std::queue<Ref<VKCommandBufferEncoder>> secondaryBuffers;
};

class LUNA_RHI_API VKDevice final : public Device {
    vk::Device m_Device;
    friend class VKSynchronization;
    friend class VKSwapchain;
    friend class VKQueue;
    friend class VKBuffer;
    friend class VKTexture;
    friend class VKTextureView;
    friend class VKSurface;
    friend class VKSampler;
    friend class VKDescriptorPool;
    friend class VKDescriptorSetLayout;
    friend class VKDescriptorSet;
    friend class VKShaderModule;
    friend class VKPipelineLayout;
    friend class VKPipelineCache;
    friend class VKGraphicsPipeline;
    friend class VKComputePipeline;
    friend class VKQueryPool;
    friend class VKTimelineSemaphore;
    friend class VKCommandBufferEncoder;
    friend class VKAccelerationStructure;
    friend class VKShaderBindingTable;
    friend class VKRayTracingPipeline;

    vk::Device& GetHandle()
    {
        return m_Device;
    }

    std::vector<uint32_t> m_queueFamilyIndices;
    Ref<Adapter> m_parentAdapter;
    std::mutex m_commandPoolMutex;
    std::unordered_map<std::thread::id, ThreadCommandPoolData> m_threadCommandPools;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    ThreadCommandPoolData& GetThreadCommandPool();
    VmaAllocator m_allocator;

    VmaAllocator& GetVmaAllocator()
    {
        return m_allocator;
    }

public:
    static Ref<VKDevice> Create(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo);
    VKDevice(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo);
    ~VKDevice() override;
    Ref<Queue> GetQueue(QueueType type, uint32_t index) override;
    Ref<Swapchain> CreateSwapchain(const SwapchainCreateInfo& createInfo) override;
    std::vector<uint32_t> GetAllQueueFamilyIndices() const override;
    Ref<Adapter> GetParentAdapter() const override;
    Ref<CommandBufferEncoder> CreateCommandBufferEncoder(CommandBufferType type = CommandBufferType::Primary) override;
    void ResetCommandPool() override;
    void ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
    void FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
    void ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder) override;
    Ref<Texture> CreateTexture(const TextureCreateInfo& createInfo) override;
    Ref<Buffer> CreateBuffer(const BufferCreateInfo& createInfo) override;
    Ref<Sampler> CreateSampler(const SamplerCreateInfo& createInfo) override;
    std::shared_ptr<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info) override;
    std::shared_ptr<DescriptorPool> CreateDescriptorPool(const DescriptorPoolCreateInfo& info) override;
    Ref<ShaderModule> CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info) override;
    Ref<PipelineLayout> CreatePipelineLayout(const PipelineLayoutCreateInfo& info) override;
    Ref<PipelineCache> CreatePipelineCache(std::span<const uint8_t> initialData) override;
    Ref<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) override;
    Ref<ComputePipeline> CreateComputePipeline(const ComputePipelineCreateInfo& info) override;
    Ref<Synchronization> CreateSynchronization(uint32_t maxFramesInFlight) override;
    Ref<QueryPool> CreateQueryPool(const QueryPoolCreateInfo& info) override;

    Ref<AccelerationStructure> CreateAccelerationStructure(const AccelerationStructureCreateInfo& info) override;
    Ref<RayTracingPipeline> CreateRayTracingPipeline(const RayTracingPipelineCreateInfo& info) override;
    Ref<ShaderBindingTable> CreateShaderBindingTable(const Ref<RayTracingPipeline>& pipeline,
                                                     uint32_t rayGenCount,
                                                     uint32_t missCount,
                                                     uint32_t hitGroupCount,
                                                     uint32_t callableCount) override;

    const vk::Device& GetNativeHandle() const
    {
        return m_Device;
    }
};
} // namespace luna::RHI
#endif
