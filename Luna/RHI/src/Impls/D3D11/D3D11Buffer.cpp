#include "Impls/D3D11/D3D11Buffer.h"
#include "Impls/D3D11/D3D11Device.h"

namespace Cacao {
D3D11Buffer::D3D11Buffer(Ref<D3D11Device> device, const BufferCreateInfo& createInfo)
    : m_device(std::move(device)),
      m_size(createInfo.Size),
      m_usage(createInfo.Usage),
      m_memoryUsage(createInfo.MemoryUsage)
{
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(m_size);
    desc.Usage = D3D11_ToUsage(m_memoryUsage);
    desc.BindFlags = 0;

    if (m_usage & BufferUsageFlags::VertexBuffer) {
        desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (m_usage & BufferUsageFlags::IndexBuffer) {
        desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
    }
    if (m_usage & BufferUsageFlags::UniformBuffer) {
        desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
    }
    if (m_usage & BufferUsageFlags::StorageBuffer) {
        desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    if (desc.BindFlags == 0 && desc.Usage == D3D11_USAGE_DYNAMIC) {
        desc.Usage = D3D11_USAGE_STAGING;
    }

    if (m_usage & BufferUsageFlags::StorageBuffer) {
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        m_needsStagingForMap = true;
    } else if (desc.Usage == D3D11_USAGE_DYNAMIC) {
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else if (desc.Usage == D3D11_USAGE_STAGING) {
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    }

    D3D11_SUBRESOURCE_DATA initData{};
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (createInfo.InitialData) {
        initData.pSysMem = createInfo.InitialData;
        pInitData = &initData;
    }

    m_device->GetNativeDevice()->CreateBuffer(&desc, pInitData, &m_buffer);
}

void* D3D11Buffer::Map()
{
    if (m_isMapped) {
        return m_needsStagingForMap ? m_cpuStagingData.data() : m_mappedData.pData;
    }
    if (!m_buffer) {
        return nullptr;
    }

    if (m_needsStagingForMap) {
        if (m_cpuStagingData.empty()) {
            m_cpuStagingData.resize(m_size, 0);
        }
        m_isMapped = true;
        return m_cpuStagingData.data();
    }

    D3D11_BUFFER_DESC desc;
    m_buffer->GetDesc(&desc);
    D3D11_MAP mapType = (desc.Usage == D3D11_USAGE_DYNAMIC) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_READ_WRITE;

    HRESULT hr = m_device->GetImmediateContext()->Map(m_buffer.Get(), 0, mapType, 0, &m_mappedData);
    if (FAILED(hr)) {
        return nullptr;
    }
    m_isMapped = true;
    return m_mappedData.pData;
}

void D3D11Buffer::Unmap()
{
    if (!m_isMapped) {
        return;
    }
    if (m_needsStagingForMap) {
        m_device->GetImmediateContext()->UpdateSubresource(m_buffer.Get(), 0, nullptr, m_cpuStagingData.data(), 0, 0);
    } else {
        m_device->GetImmediateContext()->Unmap(m_buffer.Get(), 0);
    }
    m_isMapped = false;
    m_mappedData = {};
}

void D3D11Buffer::Flush(uint64_t offset, uint64_t size)
{
    if (m_needsStagingForMap && !m_cpuStagingData.empty()) {
        m_device->GetImmediateContext()->UpdateSubresource(m_buffer.Get(), 0, nullptr, m_cpuStagingData.data(), 0, 0);
        if (m_structuredBuffer) {
            m_device->GetImmediateContext()->CopyResource(m_structuredBuffer.Get(), m_buffer.Get());
        }
    }
}

void D3D11Buffer::EnsureStructuredSRV(uint32_t stride)
{
    if (m_structuredSRV && m_structuredStride == stride) {
        return;
    }
    m_structuredStride = stride;
    fprintf(stderr, "D3D11Buffer::EnsureStructuredSRV stride=%u size=%llu\n", stride, m_size);

    auto* dev = m_device->GetNativeDevice();

    D3D11_BUFFER_DESC srcDesc{};
    m_buffer->GetDesc(&srcDesc);

    if (!(srcDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)) {
        D3D11_BUFFER_DESC structDesc = srcDesc;
        structDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        structDesc.StructureByteStride = stride;
        structDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        structDesc.Usage = D3D11_USAGE_DEFAULT;
        structDesc.CPUAccessFlags = 0;
        dev->CreateBuffer(&structDesc, nullptr, &m_structuredBuffer);
        if (m_structuredBuffer) {
            m_device->GetImmediateContext()->CopyResource(m_structuredBuffer.Get(), m_buffer.Get());
        }
    }

    ID3D11Buffer* targetBuf = m_structuredBuffer ? m_structuredBuffer.Get() : m_buffer.Get();

    D3D11_SHADER_RESOURCE_VIEW_DESC d3d11SrvDesc{};
    d3d11SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    d3d11SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    d3d11SrvDesc.Buffer.FirstElement = 0;
    d3d11SrvDesc.Buffer.NumElements = static_cast<UINT>(m_size / stride);
    HRESULT hr = dev->CreateShaderResourceView(targetBuf, &d3d11SrvDesc, &m_structuredSRV);
    fprintf(stderr,
            "  CreateSRV hr=0x%08X srv=%p structured=%p\n",
            (unsigned) hr,
            m_structuredSRV.Get(),
            m_structuredBuffer.Get());
}
} // namespace Cacao
