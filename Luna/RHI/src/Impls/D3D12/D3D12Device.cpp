#include "Impls/D3D12/D3D12AccelerationStructure.h"
#include "Impls/D3D12/D3D12Adapter.h"
#include "Impls/D3D12/D3D12Buffer.h"
#include "Impls/D3D12/D3D12CommandBufferEncoder.h"
#include "Impls/D3D12/D3D12DescriptorPool.h"
#include "Impls/D3D12/D3D12DescriptorSetLayout.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Pipeline.h"
#include "Impls/D3D12/D3D12PipelineCache.h"
#include "Impls/D3D12/D3D12PipelineLayout.h"
#include "Impls/D3D12/D3D12QueryPool.h"
#include "Impls/D3D12/D3D12Queue.h"
#include "Impls/D3D12/D3D12RayTracingPipeline.h"
#include "Impls/D3D12/D3D12Sampler.h"
#include "Impls/D3D12/D3D12ShaderBindingTable.h"
#include "Impls/D3D12/D3D12ShaderModule.h"
#include "Impls/D3D12/D3D12Swapchain.h"
#include "Impls/D3D12/D3D12Synchronization.h"
#include "Impls/D3D12/D3D12Texture.h"
#include "Impls/D3D12/D3D12TimelineSemaphore.h"

#include <fstream>

namespace luna::RHI {
D3D12Device::D3D12Device(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo)
    : m_parentAdapter(adapter)
{
    auto d3dAdapter = std::dynamic_pointer_cast<D3D12Adapter>(adapter);

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }

    HRESULT hr = D3D12CreateDevice(d3dAdapter->GetHandle(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    if (FAILED(hr)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "D3D12CreateDevice failed: 0x%08X", static_cast<unsigned>(hr));
        throw std::runtime_error(buf);
    }

    HRESULT removedReason = m_device->GetDeviceRemovedReason();
    if (FAILED(removedReason)) {
        char buf[128];
        snprintf(
            buf, sizeof(buf), "Device already removed after creation: 0x%08X", static_cast<unsigned>(removedReason));
        throw std::runtime_error(buf);
    }

    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice = m_device.Get();
    allocatorDesc.pAdapter = d3dAdapter->GetHandle();
    allocatorDesc.Flags = static_cast<D3D12MA::ALLOCATOR_FLAGS>(D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS);
    D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator);

    CreateDescriptorHeaps();
    InitPipelineLibrary();

    removedReason = m_device->GetDeviceRemovedReason();
    if (FAILED(removedReason)) {
        char buf[128];
        snprintf(buf,
                 sizeof(buf),
                 "Device removed after CreateDescriptorHeaps: 0x%08X",
                 static_cast<unsigned>(removedReason));
        throw std::runtime_error(buf);
    }
}

D3D12Device::~D3D12Device()
{
    SavePipelineLibrary();
    ReleaseAllDeferredDescriptors();
    m_threadCommandData.clear();
    if (m_allocator) {
        m_allocator->Release();
        m_allocator = nullptr;
    }
}

void D3D12Device::InitPipelineLibrary()
{
    std::ifstream file("pso_cache.bin", std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        auto size = file.tellg();
        if (size > 0) {
            m_psoLibraryBlob.resize(static_cast<size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char*>(m_psoLibraryBlob.data()), size);
            HRESULT hr = m_device->CreatePipelineLibrary(
                m_psoLibraryBlob.data(), m_psoLibraryBlob.size(), IID_PPV_ARGS(&m_pipelineLibrary));
            if (SUCCEEDED(hr)) {
                return;
            }
        }
    }
    m_device->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_pipelineLibrary));
}

void D3D12Device::SavePipelineLibrary()
{
    if (!m_pipelineLibrary) {
        return;
    }
    auto size = m_pipelineLibrary->GetSerializedSize();
    if (size == 0) {
        return;
    }
    std::vector<uint8_t> blob(size);
    if (SUCCEEDED(m_pipelineLibrary->Serialize(blob.data(), size))) {
        std::ofstream file("pso_cache.bin", std::ios::binary);
        file.write(reinterpret_cast<const char*>(blob.data()), size);
    }
}

void D3D12Device::InitializeDeferredDescriptorRecycling(uint32_t maxFramesInFlight)
{
    std::lock_guard lock(m_deferredDescriptorMutex);
    m_deferredDescriptorFrees.clear();
    m_deferredDescriptorFrees.resize(maxFramesInFlight);
    m_deferredSamplerRangeFrees.clear();
    m_deferredSamplerRangeFrees.resize(maxFramesInFlight);
    m_orphanedSamplerRangeFrees.clear();
    m_currentDeferredDescriptorFrame = kInvalidDeferredDescriptorFrame;
    m_currentDeferredDescriptorGeneration = 0;
}

void D3D12Device::PrepareDeferredDescriptorFrame(uint32_t frameIndex)
{
    std::vector<DeferredDescriptorFree> readyToFree;
    std::vector<DescriptorRange> readySamplerRanges;

    {
        std::lock_guard lock(m_deferredDescriptorMutex);
        if (frameIndex >= m_deferredDescriptorFrees.size()) {
            m_currentDeferredDescriptorFrame = kInvalidDeferredDescriptorFrame;
            return;
        }

        m_currentDeferredDescriptorFrame = frameIndex;
        ++m_currentDeferredDescriptorGeneration;
        readyToFree.swap(m_deferredDescriptorFrees[frameIndex]);
        if (frameIndex < m_deferredSamplerRangeFrees.size()) {
            readySamplerRanges.swap(m_deferredSamplerRangeFrees[frameIndex]);
        }
    }

    for (const auto& pending : readyToFree) {
        FreeDescriptorImmediate(pending.type, pending.handle);
    }

    for (const auto& range : readySamplerRanges) {
        FreeSamplerRangeImmediate(range);
    }
}

void D3D12Device::ReleaseAllDeferredDescriptors()
{
    std::vector<DeferredDescriptorFree> pendingDescriptors;
    std::vector<DescriptorRange> pendingSamplerRanges;

    {
        std::lock_guard lock(m_deferredDescriptorMutex);
        for (auto& pendingList : m_deferredDescriptorFrees) {
            pendingDescriptors.insert(pendingDescriptors.end(), pendingList.begin(), pendingList.end());
            pendingList.clear();
        }
        for (auto& pendingList : m_deferredSamplerRangeFrees) {
            pendingSamplerRanges.insert(pendingSamplerRanges.end(), pendingList.begin(), pendingList.end());
            pendingList.clear();
        }
        pendingSamplerRanges.insert(
            pendingSamplerRanges.end(), m_orphanedSamplerRangeFrees.begin(), m_orphanedSamplerRangeFrees.end());
        m_orphanedSamplerRangeFrees.clear();
        m_currentDeferredDescriptorFrame = kInvalidDeferredDescriptorFrame;
    }

    for (const auto& pending : pendingDescriptors) {
        FreeDescriptorImmediate(pending.type, pending.handle);
    }

    for (const auto& range : pendingSamplerRanges) {
        FreeSamplerRangeImmediate(range);
    }
}

void D3D12Device::QueueDeferredDescriptorFree(DeferredDescriptorType type, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    if (handle.ptr == 0) {
        return;
    }

    {
        std::lock_guard lock(m_deferredDescriptorMutex);
        if (m_currentDeferredDescriptorFrame != kInvalidDeferredDescriptorFrame &&
            m_currentDeferredDescriptorFrame < m_deferredDescriptorFrees.size()) {
            m_deferredDescriptorFrees[m_currentDeferredDescriptorFrame].push_back({type, handle});
            return;
        }
    }

    FreeDescriptorImmediate(type, handle);
}

void D3D12Device::QueueDeferredSamplerRangeFree(DescriptorRange range)
{
    if (!range.IsValid()) {
        return;
    }

    {
        std::lock_guard lock(m_deferredDescriptorMutex);
        if (m_currentDeferredDescriptorFrame != kInvalidDeferredDescriptorFrame &&
            m_currentDeferredDescriptorFrame < m_deferredSamplerRangeFrees.size()) {
            m_deferredSamplerRangeFrees[m_currentDeferredDescriptorFrame].push_back(range);
            return;
        }

        m_orphanedSamplerRangeFrees.push_back(range);
    }
}

void D3D12Device::FreeDescriptorImmediate(DeferredDescriptorType type, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    switch (type) {
        case DeferredDescriptorType::RTV:
            m_rtvPool.Free(handle);
            break;
        case DeferredDescriptorType::DSV:
            m_dsvPool.Free(handle);
            break;
        case DeferredDescriptorType::CbvSrvUav:
            m_cbvSrvUavPool.Free(handle);
            break;
        default:
            break;
    }
}

void D3D12Device::FreeSamplerRangeImmediate(DescriptorRange range)
{
    m_samplerRangeAllocator.Free(range);
}

D3D12Device::SamplerTableAllocation D3D12Device::AllocateTransientSamplerTable(uint32_t count)
{
    SamplerTableAllocation allocation{};
    if (count == 0 || !m_samplerHeap) {
        return allocation;
    }

    const DescriptorRange range = m_samplerRangeAllocator.Allocate(count);
    allocation.cpu = GetSamplerCpuHandle(range.offset);
    allocation.gpu = GetSamplerGpuHandle(range.offset);
    allocation.heap = m_samplerHeap.Get();
    allocation.count = count;

    QueueDeferredSamplerRangeFree(range);
    return allocation;
}

void D3D12Device::StorePSO(const wchar_t* name, ID3D12PipelineState* pso)
{
    if (!m_pipelineLibrary || !pso) {
        return;
    }
    std::lock_guard lock(m_psoLibMutex);
    m_pipelineLibrary->StorePipeline(name, pso);
}

Ref<D3D12Device> D3D12Device::Create(const Ref<Adapter>& adapter, const DeviceCreateInfo& createInfo)
{
    return std::make_shared<D3D12Device>(adapter, createInfo);
}

void D3D12Device::CreateDescriptorHeaps()
{
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    auto createHeap =
        [&](D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shaderVisible, ComPtr<ID3D12DescriptorHeap>& heap) {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.NumDescriptors = count;
            desc.Type = type;
            desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
        };

    createHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, false, m_rtvHeap);
    createHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, false, m_dsvHeap);
    createHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65'536, false, m_cbvSrvUavHeap);
    createHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2'048, true, m_samplerHeap);

    auto initPool =
        [](DescriptorPoolAllocator& pool, ComPtr<ID3D12DescriptorHeap>& heap, uint32_t descSize, uint32_t capacity) {
            pool.heap = heap;
            pool.descriptorSize = descSize;
            pool.capacity = capacity;
            pool.next.store(0);
        };
    initPool(m_rtvPool, m_rtvHeap, m_rtvDescriptorSize, 256);
    initPool(m_dsvPool, m_dsvHeap, m_dsvDescriptorSize, 64);
    initPool(m_cbvSrvUavPool, m_cbvSrvUavHeap, m_cbvSrvUavDescriptorSize, 65'536);
    m_samplerRangeAllocator.capacity = 2'048;
    m_samplerRangeAllocator.Reset();
}

ComPtr<ID3D12CommandAllocator> D3D12Device::AcquireCommandAllocator()
{
    std::lock_guard lock(m_commandAllocatorMutex);
    if (!m_freeAllocators.empty()) {
        auto alloc = std::move(m_freeAllocators.back());
        m_freeAllocators.pop_back();
        alloc->Reset();
        return alloc;
    }
    ComPtr<ID3D12CommandAllocator> alloc;
    m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    return alloc;
}

void D3D12Device::RecycleCommandAllocator(ComPtr<ID3D12CommandAllocator> alloc)
{
    std::lock_guard lock(m_commandAllocatorMutex);
    m_freeAllocators.push_back(std::move(alloc));
}

D3D12Device::ThreadCommandData& D3D12Device::GetThreadCommandData()
{
    auto id = std::this_thread::get_id();
    std::lock_guard lock(m_commandAllocatorMutex);
    auto it = m_threadCommandData.find(id);
    if (it == m_threadCommandData.end()) {
        ThreadCommandData data;
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&data.allocator));
        m_threadCommandData[id] = std::move(data);
        return m_threadCommandData[id];
    }
    return it->second;
}

Ref<Queue> D3D12Device::GetQueue(QueueType type, uint32_t index)
{
    uint64_t key = (static_cast<uint64_t>(type) << 32) | index;
    auto it = m_queueCache.find(key);
    if (it != m_queueCache.end()) {
        return it->second;
    }
    auto q = std::make_shared<D3D12Queue>(shared_from_this(), type);
    m_queueCache[key] = q;
    return q;
}

Ref<Swapchain> D3D12Device::CreateSwapchain(const SwapchainCreateInfo& createInfo)
{
    return std::make_shared<D3D12Swapchain>(shared_from_this(), createInfo);
}

std::vector<uint32_t> D3D12Device::GetAllQueueFamilyIndices() const
{
    return {0};
}

Ref<Adapter> D3D12Device::GetParentAdapter() const
{
    return m_parentAdapter;
}

Ref<CommandBufferEncoder> D3D12Device::CreateCommandBufferEncoder(CommandBufferType type)
{
    auto& threadData = GetThreadCommandData();
    auto& encoderPool =
        (type == CommandBufferType::Secondary) ? threadData.secondaryEncoders : threadData.primaryEncoders;
    if (!encoderPool.empty()) {
        auto encoder = encoderPool.back();
        encoderPool.pop_back();
        return encoder;
    }

    auto allocator = AcquireCommandAllocator();
    return std::make_shared<D3D12CommandBufferEncoder>(shared_from_this(), type, allocator);
}

void D3D12Device::ResetCommandPool()
{
    auto id = std::this_thread::get_id();
    std::lock_guard lock(m_commandAllocatorMutex);
    auto it = m_threadCommandData.find(id);
    if (it != m_threadCommandData.end()) {
        it->second.allocator->Reset();
    }
}

void D3D12Device::ReturnCommandBuffer(const Ref<CommandBufferEncoder>& encoder)
{
    auto d3dEncoder = std::dynamic_pointer_cast<D3D12CommandBufferEncoder>(encoder);
    if (!d3dEncoder) {
        return;
    }

    d3dEncoder->Reset();

    auto& threadData = GetThreadCommandData();
    auto& encoderPool = (d3dEncoder->GetCommandBufferType() == CommandBufferType::Secondary)
                            ? threadData.secondaryEncoders
                            : threadData.primaryEncoders;
    encoderPool.push_back(std::move(d3dEncoder));
}

void D3D12Device::FreeCommandBuffer(const Ref<CommandBufferEncoder>& encoder)
{
    auto d3dEncoder = std::dynamic_pointer_cast<D3D12CommandBufferEncoder>(encoder);
    if (d3dEncoder) {
        d3dEncoder->Reset();
    }
}

void D3D12Device::ResetCommandBuffer(const Ref<CommandBufferEncoder>& encoder)
{
    encoder->Reset();
}

Ref<Texture> D3D12Device::CreateTexture(const TextureCreateInfo& info)
{
    return std::make_shared<D3D12Texture>(shared_from_this(), info);
}

Ref<Buffer> D3D12Device::CreateBuffer(const BufferCreateInfo& info)
{
    return D3D12Buffer::Create(shared_from_this(), info);
}

Ref<Sampler> D3D12Device::CreateSampler(const SamplerCreateInfo& info)
{
    return std::make_shared<D3D12Sampler>(shared_from_this(), info);
}

std::shared_ptr<DescriptorSetLayout> D3D12Device::CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
{
    return std::make_shared<D3D12DescriptorSetLayout>(shared_from_this(), info);
}

std::shared_ptr<DescriptorPool> D3D12Device::CreateDescriptorPool(const DescriptorPoolCreateInfo& info)
{
    return std::make_shared<D3D12DescriptorPool>(shared_from_this(), info);
}

Ref<ShaderModule> D3D12Device::CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
{
    return std::make_shared<D3D12ShaderModule>(blob, info);
}

Ref<PipelineLayout> D3D12Device::CreatePipelineLayout(const PipelineLayoutCreateInfo& info)
{
    return std::make_shared<D3D12PipelineLayout>(shared_from_this(), info);
}

Ref<PipelineCache> D3D12Device::CreatePipelineCache(std::span<const uint8_t> initialData)
{
    return std::make_shared<D3D12PipelineCache>(shared_from_this(), initialData);
}

Ref<GraphicsPipeline> D3D12Device::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info)
{
    ValidateGraphicsPipeline(info);
    return std::make_shared<D3D12GraphicsPipeline>(shared_from_this(), info);
}

Ref<ComputePipeline> D3D12Device::CreateComputePipeline(const ComputePipelineCreateInfo& info)
{
    return std::make_shared<D3D12ComputePipeline>(shared_from_this(), info);
}

Ref<Synchronization> D3D12Device::CreateSynchronization(uint32_t maxFramesInFlight)
{
    return std::make_shared<D3D12Synchronization>(shared_from_this(), maxFramesInFlight);
}

Ref<QueryPool> D3D12Device::CreateQueryPool(const QueryPoolCreateInfo& info)
{
    return std::make_shared<D3D12QueryPool>(shared_from_this(), info);
}

Ref<AccelerationStructure> D3D12Device::CreateAccelerationStructure(const AccelerationStructureCreateInfo& info)
{
    return std::make_shared<D3D12AccelerationStructure>(shared_from_this(), info);
}

Ref<RayTracingPipeline> D3D12Device::CreateRayTracingPipeline(const RayTracingPipelineCreateInfo& info)
{
    return std::make_shared<D3D12RayTracingPipeline>(shared_from_this(), info);
}

Ref<ShaderBindingTable> D3D12Device::CreateShaderBindingTable(const Ref<RayTracingPipeline>& pipeline,
                                                              uint32_t rayGenCount,
                                                              uint32_t missCount,
                                                              uint32_t hitGroupCount,
                                                              uint32_t callableCount)
{
    auto* d3dPipeline = static_cast<D3D12RayTracingPipeline*>(pipeline.get());
    return std::make_shared<D3D12ShaderBindingTable>(
        shared_from_this(), d3dPipeline->GetStateObject(), rayGenCount, missCount, hitGroupCount, callableCount);
}

Ref<TimelineSemaphore> D3D12Device::CreateTimelineSemaphore(uint64_t initialValue)
{
    return std::make_shared<D3D12TimelineSemaphore>(shared_from_this(), initialValue);
}
} // namespace luna::RHI
