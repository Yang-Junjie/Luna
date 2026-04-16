#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12PipelineCache.h"

namespace Cacao {
D3D12PipelineCache::D3D12PipelineCache(const Ref<Device>& device, std::span<const uint8_t> initialData)
    : m_device(std::dynamic_pointer_cast<D3D12Device>(device))
{
    auto* d3dDevice = m_device->GetHandle();
    const void* blobData = initialData.empty() ? nullptr : initialData.data();
    SIZE_T blobSize = initialData.size();

    HRESULT hr = d3dDevice->CreatePipelineLibrary(blobData, blobSize, IID_PPV_ARGS(&m_library));
    if (FAILED(hr) && blobData) {
        d3dDevice->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_library));
    }
}

std::vector<uint8_t> D3D12PipelineCache::GetData() const
{
    if (!m_library) {
        return {};
    }
    SIZE_T size = m_library->GetSerializedSize();
    if (size == 0) {
        return {};
    }
    std::vector<uint8_t> data(size);
    if (FAILED(m_library->Serialize(data.data(), size))) {
        return {};
    }
    return data;
}

void D3D12PipelineCache::Merge(std::span<const Ref<PipelineCache>> srcCaches)
{
    if (!m_library) {
        return;
    }

    auto currentData = GetData();
    for (const auto& cache : srcCaches) {
        auto* d3dCache = dynamic_cast<D3D12PipelineCache*>(cache.get());
        if (!d3dCache || !d3dCache->m_library) {
            continue;
        }

        SIZE_T srcSize = d3dCache->m_library->GetSerializedSize();
        if (srcSize == 0) {
            continue;
        }
        std::vector<uint8_t> srcData(srcSize);
        if (FAILED(d3dCache->m_library->Serialize(srcData.data(), srcSize))) {
            continue;
        }

        ComPtr<ID3D12PipelineLibrary1> srcLib;
        HRESULT hr =
            m_device->GetHandle()->CreatePipelineLibrary(srcData.data(), srcData.size(), IID_PPV_ARGS(&srcLib));
        if (FAILED(hr)) {
            continue;
        }

        // D3D12 has no native merge; re-store each PSO from source into our library.
        // The pipeline library API doesn't expose enumeration, so merge is a best-effort
        // operation that retains the current library's content. Full merge requires the
        // application to re-store PSOs through the device's StorePSO mechanism.
    }
}
} // namespace Cacao
