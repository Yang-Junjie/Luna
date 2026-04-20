#ifndef LUNA_RHI_D3D12DESCRIPTORSET_H
#define LUNA_RHI_D3D12DESCRIPTORSET_H
#include "D3D12Common.h"
#include "DescriptorSet.h"

#include <limits>
#include <unordered_map>
#include <vector>

namespace luna::RHI {
class D3D12DescriptorPool;
class D3D12Device;

class LUNA_RHI_API D3D12DescriptorSet : public DescriptorSet {
public:
    struct SlotInfo {
        uint32_t binding;
        bool isSampler;
        uint32_t slot;
    };

private:
    Ref<Device> m_device;

    D3D12_GPU_DESCRIPTOR_HANDLE m_cbvSrvUavGpu = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_cbvSrvUavCpu = {};
    uint32_t m_cbvSrvUavCount = 0;
    uint32_t m_cbvSrvUavDescSize = 0;

    uint32_t m_samplerCount = 0;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_samplerDescriptors;
    D3D12_GPU_DESCRIPTOR_HANDLE m_samplerGpu = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_samplerCpu = {};
    uint32_t m_stagedSamplerFrameIndex = (std::numeric_limits<uint32_t>::max)();
    uint64_t m_samplerVersion = 1;
    uint64_t m_stagedSamplerVersion = 0;

    std::vector<SlotInfo> m_bindingMap;

    ID3D12DescriptorHeap* m_cbvSrvUavHeap = nullptr;
    ID3D12DescriptorHeap* m_samplerHeap = nullptr;

    std::unordered_map<uint32_t, uint64_t> m_bindingHashes;
    uint64_t m_cachedHash = 0;
    bool m_dirty = true;

    const SlotInfo* FindSlot(uint32_t binding) const;

public:
    D3D12DescriptorSet(const Ref<Device>& device,
                       D3D12_GPU_DESCRIPTOR_HANDLE cbvGpu,
                       D3D12_CPU_DESCRIPTOR_HANDLE cbvCpu,
                       uint32_t cbvCount,
                       uint32_t cbvDescSize,
                       uint32_t sampCount,
                       std::vector<SlotInfo> bindingMap,
                       ID3D12DescriptorHeap* cbvHeap);

    D3D12_GPU_DESCRIPTOR_HANDLE GetCBVSRVUAVGPUHandle() const
    {
        return m_cbvSrvUavGpu;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGPUHandle() const
    {
        return m_samplerGpu;
    }

    bool HasCBVSRVUAV() const
    {
        return m_cbvSrvUavCount > 0;
    }

    bool HasSamplers() const
    {
        return m_samplerCount > 0;
    }

    ID3D12DescriptorHeap* GetCBVSRVUAVHeap() const
    {
        return m_cbvSrvUavHeap;
    }

    ID3D12DescriptorHeap* GetSamplerHeap() const
    {
        return m_samplerHeap;
    }

    bool PrepareSamplerTable(D3D12Device& device);

    void Update() override {}

    void WriteBuffer(const BufferWriteInfo& info) override;
    void WriteTexture(const TextureWriteInfo& info) override;
    void WriteSampler(const SamplerWriteInfo& info) override;
    void WriteAccelerationStructure(const AccelerationStructureWriteInfo& info) override;

    void WriteBuffers(const BufferWriteInfos& infos) override {}

    void WriteTextures(const TextureWriteInfos& infos) override {}

    void WriteSamplers(const SamplerWriteInfos& infos) override {}

    void WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos) override {}
};
} // namespace luna::RHI

#endif
