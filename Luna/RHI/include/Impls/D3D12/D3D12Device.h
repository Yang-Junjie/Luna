#ifndef LUNA_RHI_D3D12DEVICE_H
#define LUNA_RHI_D3D12DEVICE_H
#include "D3D12Common.h"
#include "D3D12MemAlloc.h"
#include "Device.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace luna::RHI {
struct DescriptorPoolAllocator {
    ComPtr<ID3D12DescriptorHeap> heap;
    uint32_t descriptorSize = 0;
    std::atomic<uint32_t> next{0};
    uint32_t capacity = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate()
    {
        uint32_t idx = next.fetch_add(1, std::memory_order_relaxed);
        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(idx) * descriptorSize;
        return handle;
    }
};

class LUNA_RHI_API D3D12Device final : public Device {
private:
    ComPtr<ID3D12Device5> m_device;
    D3D12MA::Allocator* m_allocator = nullptr;
    Ref<Adapter> m_parentAdapter;

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
    uint32_t m_rtvDescriptorSize = 0;
    uint32_t m_dsvDescriptorSize = 0;
    uint32_t m_cbvSrvUavDescriptorSize = 0;
    uint32_t m_samplerDescriptorSize = 0;

    DescriptorPoolAllocator m_rtvPool;
    DescriptorPoolAllocator m_dsvPool;
    DescriptorPoolAllocator m_cbvSrvUavPool;

    std::mutex m_commandAllocatorMutex;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_freeAllocators;

    struct ThreadCommandData {
        ComPtr<ID3D12CommandAllocator> allocator;
    };

    std::unordered_map<std::thread::id, ThreadCommandData> m_threadCommandData;

    std::unordered_map<uint64_t, Ref<Queue>> m_queueCache;

    ComPtr<ID3D12PipelineLibrary1> m_pipelineLibrary;
    std::vector<uint8_t> m_psoLibraryBlob;
    std::mutex m_psoLibMutex;
    void InitPipelineLibrary();
    void SavePipelineLibrary();

    friend class D3D12Queue;
    friend class D3D12CommandBufferEncoder;
    friend class D3D12Buffer;
    friend class D3D12Texture;
    friend class D3D12TextureView;
    friend class D3D12Swapchain;
    friend class D3D12Pipeline;
    friend class D3D12QueryPool;
    friend class D3D12Surface;
    friend class D3D12DescriptorPool;
    friend class D3D12Sampler;
    friend class D3D12ShaderModule;
    friend class D3D12PipelineLayout;
    friend class D3D12PipelineCache;
    friend class D3D12GraphicsPipeline;
    friend class D3D12ComputePipeline;
    friend class D3D12Synchronization;
    friend class D3D12StagingBufferPool;
    friend class D3D12DescriptorSet;
    friend class D3D12DescriptorSetLayout;
    friend class D3D12AccelerationStructure;
    friend class D3D12RayTracingPipeline;
    friend class D3D12TimelineSemaphore;

    ID3D12Device5* GetHandle() const
    {
        return m_device.Get();
    }

    D3D12MA::Allocator* GetAllocator() const
    {
        return m_allocator;
    }

    ThreadCommandData& GetThreadCommandData();
    void CreateDescriptorHeaps();

public:
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateRTV()
    {
        return m_rtvPool.Allocate();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDSV()
    {
        return m_dsvPool.Allocate();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCbvSrvUav()
    {
        return m_cbvSrvUavPool.Allocate();
    }

    ComPtr<ID3D12CommandAllocator> AcquireCommandAllocator();
    void RecycleCommandAllocator(ComPtr<ID3D12CommandAllocator> alloc);

    ID3D12PipelineLibrary1* GetPipelineLibrary()
    {
        return m_pipelineLibrary.Get();
    }

    void StorePSO(const wchar_t* name, ID3D12PipelineState* pso);
    static Ref<D3D12Device> Create(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo);
    D3D12Device(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo);
    ~D3D12Device() override;

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

    Ref<AccelerationStructure> CreateAccelerationStructure(const AccelerationStructureCreateInfo& info) override;
    Ref<RayTracingPipeline> CreateRayTracingPipeline(const RayTracingPipelineCreateInfo& info) override;
    Ref<ShaderBindingTable> CreateShaderBindingTable(const Ref<RayTracingPipeline>& pipeline,
                                                     uint32_t rayGenCount,
                                                     uint32_t missCount,
                                                     uint32_t hitGroupCount,
                                                     uint32_t callableCount) override;

    Ref<TimelineSemaphore> CreateTimelineSemaphore(uint64_t initialValue = 0) override;
};
} // namespace luna::RHI

#endif
