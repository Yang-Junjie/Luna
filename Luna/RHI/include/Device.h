#ifndef CACAO_CACAODEVICE_H
#define CACAO_CACAODEVICE_H
#include "ShaderModule.h"

namespace Cacao {
enum class DeviceFeature : uint32_t;
class Synchronization;
} // namespace Cacao

namespace Cacao {
struct ComputePipelineCreateInfo;
class ComputePipeline;
struct QueryPoolCreateInfo;
class QueryPool;
class TimelineSemaphore;
struct AccelerationStructureCreateInfo;
class AccelerationStructure;
struct RayTracingPipelineCreateInfo;
class RayTracingPipeline;
class ShaderBindingTable;
struct GraphicsPipelineCreateInfo;
class GraphicsPipeline;
class PipelineCache;
struct PipelineLayoutCreateInfo;
class PipelineLayout;
struct DescriptorPoolCreateInfo;
class DescriptorPool;
struct DescriptorSetLayoutCreateInfo;
class DescriptorSetLayout;
struct SamplerCreateInfo;
class Sampler;
struct BufferCreateInfo;
class Buffer;
struct TextureCreateInfo;
class Texture;
class CommandBufferEncoder;
class Adapter;
struct SwapchainCreateInfo;
class Swapchain;
class Queue;
class Surface;
enum class QueueType;

struct QueueRequest {
    QueueType Type;
    uint32_t Count = 1;
    float Priority = 1.0f;
};

struct DeviceCreateInfo {
    std::vector<DeviceFeature> EnabledFeatures;
    std::vector<QueueRequest> QueueRequests;
    Ref<Surface> CompatibleSurface = nullptr;
    void* Next = nullptr;
};
enum class CommandBufferType {
    Primary,
    Secondary
};

class CACAO_API Device : public std::enable_shared_from_this<Device> {
public:
    virtual ~Device() = default;
    virtual Ref<Queue> GetQueue(QueueType type, uint32_t index = 0) = 0;
    virtual Ref<Swapchain> CreateSwapchain(const SwapchainCreateInfo& createInfo) = 0;
    virtual std::vector<uint32_t> GetAllQueueFamilyIndices() const = 0;
    virtual Ref<Adapter> GetParentAdapter() const = 0;
    virtual Ref<CommandBufferEncoder>
        CreateCommandBufferEncoder(CommandBufferType type = CommandBufferType::Primary) = 0;
    virtual void ResetCommandPool() = 0;
    virtual void ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder) = 0;
    virtual void FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder) = 0;
    virtual void ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder) = 0;
    virtual Ref<Texture> CreateTexture(const TextureCreateInfo& createInfo) = 0;
    virtual Ref<Buffer> CreateBuffer(const BufferCreateInfo& createInfo) = 0;
    virtual Ref<Sampler> CreateSampler(const SamplerCreateInfo& createInfo) = 0;
    virtual Ref<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info) = 0;
    virtual Ref<DescriptorPool> CreateDescriptorPool(const DescriptorPoolCreateInfo& info) = 0;
    virtual Ref<ShaderModule> CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info) = 0;
    virtual Ref<PipelineLayout> CreatePipelineLayout(const PipelineLayoutCreateInfo& info) = 0;
    virtual Ref<PipelineCache> CreatePipelineCache(std::span<const uint8_t> initialData = {}) = 0;
    virtual Ref<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) = 0;
    virtual Ref<ComputePipeline> CreateComputePipeline(const ComputePipelineCreateInfo& info) = 0;
    virtual Ref<Synchronization> CreateSynchronization(uint32_t maxFramesInFlight) = 0;

    virtual Ref<QueryPool> CreateQueryPool(const QueryPoolCreateInfo& info)
    {
        return nullptr;
    }

    virtual Ref<TimelineSemaphore> CreateTimelineSemaphore(uint64_t initialValue = 0)
    {
        return nullptr;
    }

    virtual Ref<AccelerationStructure> CreateAccelerationStructure(const AccelerationStructureCreateInfo& info)
    {
        return nullptr;
    }

    virtual Ref<RayTracingPipeline> CreateRayTracingPipeline(const RayTracingPipelineCreateInfo& info)
    {
        return nullptr;
    }

    virtual Ref<ShaderBindingTable> CreateShaderBindingTable(const Ref<RayTracingPipeline>& pipeline,
                                                             uint32_t rayGenCount = 1,
                                                             uint32_t missCount = 1,
                                                             uint32_t hitGroupCount = 1,
                                                             uint32_t callableCount = 0)
    {
        return nullptr;
    }

    bool ValidateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) const;
    bool ValidateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info) const;
};
} // namespace Cacao
#endif
