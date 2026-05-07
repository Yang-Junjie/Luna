#ifndef LUNA_RHI_D3D12DEVICE_H
#define LUNA_RHI_D3D12DEVICE_H
#include "D3D12Common.h"
#include "D3D12MemAlloc.h"
#include "Device.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

namespace luna::RHI {
class D3D12CommandBufferEncoder;

struct DescriptorPoolAllocator {
    ComPtr<ID3D12DescriptorHeap> heap;
    uint32_t descriptorSize = 0;
    std::atomic<uint32_t> next{0};
    uint32_t capacity = 0;
    std::mutex mutex;
    std::vector<uint32_t> freeIndices;

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate()
    {
        std::lock_guard lock(mutex);

        uint32_t idx = 0;
        if (!freeIndices.empty()) {
            idx = freeIndices.back();
            freeIndices.pop_back();
        } else {
            idx = next.fetch_add(1, std::memory_order_relaxed);
            if (idx >= capacity) {
                throw std::runtime_error("D3D12 descriptor heap exhausted");
            }
        }

        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(idx) * descriptorSize;
        return handle;
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        if (!heap || handle.ptr == 0 || descriptorSize == 0 || capacity == 0) {
            return;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE base = heap->GetCPUDescriptorHandleForHeapStart();
        if (handle.ptr < base.ptr) {
            return;
        }

        const SIZE_T delta = handle.ptr - base.ptr;
        if ((delta % descriptorSize) != 0) {
            return;
        }

        const uint32_t idx = static_cast<uint32_t>(delta / descriptorSize);
        if (idx >= capacity) {
            return;
        }

        std::lock_guard lock(mutex);
        freeIndices.push_back(idx);
    }
};

struct DescriptorRange {
    uint32_t offset = 0;
    uint32_t count = 0;

    bool IsValid() const
    {
        return count > 0;
    }
};

struct DescriptorRangeAllocator {
    uint32_t capacity = 0;
    uint32_t next = 0;
    std::mutex mutex;
    std::vector<DescriptorRange> freeRanges;

    DescriptorRange Allocate(uint32_t count)
    {
        if (count == 0) {
            return {};
        }

        std::lock_guard lock(mutex);

        for (size_t i = 0; i < freeRanges.size(); ++i) {
            auto& range = freeRanges[i];
            if (range.count < count) {
                continue;
            }

            DescriptorRange allocated{range.offset, count};
            range.offset += count;
            range.count -= count;
            if (range.count == 0) {
                freeRanges.erase(freeRanges.begin() + static_cast<std::ptrdiff_t>(i));
            }
            return allocated;
        }

        const uint64_t required = static_cast<uint64_t>(next) + count;
        if (required > capacity) {
            throw std::runtime_error("D3D12 descriptor range allocator exhausted");
        }

        DescriptorRange allocated{next, count};
        next += count;
        return allocated;
    }

    void Free(DescriptorRange range)
    {
        if (!range.IsValid() || capacity == 0) {
            return;
        }

        const uint64_t end = static_cast<uint64_t>(range.offset) + range.count;
        if (range.offset >= capacity || end > capacity) {
            return;
        }

        std::lock_guard lock(mutex);

        auto it = freeRanges.begin();
        while (it != freeRanges.end() && it->offset < range.offset) {
            ++it;
        }

        it = freeRanges.insert(it, range);

        if (it != freeRanges.begin()) {
            auto prev = std::prev(it);
            const uint64_t prevEnd = static_cast<uint64_t>(prev->offset) + prev->count;
            if (prevEnd >= it->offset) {
                const uint64_t mergedEnd = (std::max)(prevEnd, static_cast<uint64_t>(it->offset) + it->count);
                prev->count = static_cast<uint32_t>(mergedEnd - prev->offset);
                it = freeRanges.erase(it);
                it = prev;
            }
        }

        auto nextIt = std::next(it);
        while (nextIt != freeRanges.end()) {
            const uint64_t currentEnd = static_cast<uint64_t>(it->offset) + it->count;
            if (currentEnd < nextIt->offset) {
                break;
            }

            const uint64_t mergedEnd = (std::max)(currentEnd, static_cast<uint64_t>(nextIt->offset) + nextIt->count);
            it->count = static_cast<uint32_t>(mergedEnd - it->offset);
            nextIt = freeRanges.erase(nextIt);
        }
    }

    void Reset()
    {
        std::lock_guard lock(mutex);
        next = 0;
        freeRanges.clear();
    }
};

class LUNA_RHI_API D3D12Device final : public Device {
private:
    enum class DeferredDescriptorType : uint8_t {
        RTV,
        DSV,
        CbvSrvUav,
    };

    struct DeferredDescriptorFree {
        DeferredDescriptorType type{};
        D3D12_CPU_DESCRIPTOR_HANDLE handle{};
    };

    struct SamplerTableAllocation {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        ID3D12DescriptorHeap* heap = nullptr;
        uint32_t count = 0;

        bool IsValid() const
        {
            return heap != nullptr && count > 0;
        }
    };

    static constexpr uint32_t kInvalidDeferredDescriptorFrame = (std::numeric_limits<uint32_t>::max)();

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
    DescriptorRangeAllocator m_samplerRangeAllocator;

    std::mutex m_commandAllocatorMutex;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_freeAllocators;

    struct ThreadCommandData {
        ComPtr<ID3D12CommandAllocator> allocator;
        std::vector<Ref<D3D12CommandBufferEncoder>> primaryEncoders;
        std::vector<Ref<D3D12CommandBufferEncoder>> secondaryEncoders;
    };

    std::unordered_map<std::thread::id, ThreadCommandData> m_threadCommandData;

    std::unordered_map<uint64_t, Ref<Queue>> m_queueCache;

    ComPtr<ID3D12PipelineLibrary1> m_pipelineLibrary;
    std::vector<uint8_t> m_psoLibraryBlob;
    std::mutex m_psoLibMutex;
    std::mutex m_deferredDescriptorMutex;
    std::vector<std::vector<DeferredDescriptorFree>> m_deferredDescriptorFrees;
    std::vector<std::vector<DescriptorRange>> m_deferredSamplerRangeFrees;
    std::vector<DescriptorRange> m_orphanedSamplerRangeFrees;
    uint32_t m_currentDeferredDescriptorFrame = kInvalidDeferredDescriptorFrame;
    uint64_t m_currentDeferredDescriptorGeneration = 0;
    void InitPipelineLibrary();
    void SavePipelineLibrary();
    void InitializeDeferredDescriptorRecycling(uint32_t maxFramesInFlight);
    void PrepareDeferredDescriptorFrame(uint32_t frameIndex);
    void ReleaseAllDeferredDescriptors();
    void QueueDeferredDescriptorFree(DeferredDescriptorType type, D3D12_CPU_DESCRIPTOR_HANDLE handle);
    void QueueDeferredSamplerRangeFree(DescriptorRange range);
    void FreeDescriptorImmediate(DeferredDescriptorType type, D3D12_CPU_DESCRIPTOR_HANDLE handle);
    void FreeSamplerRangeImmediate(DescriptorRange range);

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

    void FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        QueueDeferredDescriptorFree(DeferredDescriptorType::RTV, handle);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDSV()
    {
        return m_dsvPool.Allocate();
    }

    void FreeDSV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        QueueDeferredDescriptorFree(DeferredDescriptorType::DSV, handle);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCbvSrvUav()
    {
        return m_cbvSrvUavPool.Allocate();
    }

    void FreeCbvSrvUav(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        QueueDeferredDescriptorFree(DeferredDescriptorType::CbvSrvUav, handle);
    }

    SamplerTableAllocation AllocateTransientSamplerTable(uint32_t count);

    D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCpuHandle(uint32_t offset) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            m_samplerHeap ? m_samplerHeap->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{};
        handle.ptr += static_cast<SIZE_T>(offset) * m_samplerDescriptorSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpuHandle(uint32_t offset) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle =
            m_samplerHeap ? m_samplerHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};
        handle.ptr += static_cast<UINT64>(offset) * m_samplerDescriptorSize;
        return handle;
    }

    uint32_t GetSamplerDescriptorSize() const
    {
        return m_samplerDescriptorSize;
    }

    uint32_t GetCurrentDeferredDescriptorFrameIndex() const
    {
        return m_currentDeferredDescriptorFrame;
    }

    uint64_t GetCurrentDeferredDescriptorGeneration() const
    {
        return m_currentDeferredDescriptorGeneration;
    }

    ID3D12DescriptorHeap* GetShaderVisibleSamplerHeap() const
    {
        return m_samplerHeap.Get();
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
    Ref<QueryPool> CreateQueryPool(const QueryPoolCreateInfo& info) override;

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
