#ifndef CACAO_D3D12PIPELINECACHE_H
#define CACAO_D3D12PIPELINECACHE_H

#include "D3D12Common.h"
#include "Device.h"
#include "Pipeline.h"

namespace Cacao {
class D3D12Device;

class CACAO_API D3D12PipelineCache final : public PipelineCache {
private:
    ComPtr<ID3D12PipelineLibrary1> m_library;
    Ref<D3D12Device> m_device;

public:
    D3D12PipelineCache(const Ref<Device>& device, std::span<const uint8_t> initialData);
    ~D3D12PipelineCache() override = default;

    std::vector<uint8_t> GetData() const override;
    void Merge(std::span<const Ref<PipelineCache>> srcCaches) override;

    ID3D12PipelineLibrary1* GetHandle() const
    {
        return m_library.Get();
    }
};
} // namespace Cacao

#endif
