#include "Impls/D3D12/D3D12Texture.h"
#include "Impls/D3D12/D3D12Device.h"

namespace Cacao
{
    D3D12Texture::D3D12Texture(const Ref<Device>& device, const TextureCreateInfo& info)
        : m_device(device), m_createInfo(info), m_ownsResource(true),
          m_currentState(info.InitialState)
    {
        auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = info.Width;
        desc.Height = info.Height;
        desc.DepthOrArraySize = static_cast<UINT16>(std::max(info.Depth, info.ArrayLayers));
        desc.MipLevels = static_cast<UINT16>(info.MipLevels);
        desc.Format = ToDXGIFormat(info.Format);
        desc.SampleDesc.Count = static_cast<UINT>(info.SampleCount);
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        if (info.Usage & TextureUsageFlags::ColorAttachment)
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (info.Usage & TextureUsageFlags::DepthStencilAttachment)
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (info.Usage & TextureUsageFlags::Storage)
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        bool depthSampled = (info.Usage & TextureUsageFlags::DepthStencilAttachment)
                         && (info.Usage & TextureUsageFlags::Sampled);
        if (depthSampled)
        {
            if (desc.Format == DXGI_FORMAT_D32_FLOAT)
                desc.Format = DXGI_FORMAT_R32_TYPELESS;
            else if (desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
                desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
            else if (desc.Format == DXGI_FORMAT_D16_UNORM)
                desc.Format = DXGI_FORMAT_R16_TYPELESS;
            else if (desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
                desc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
        }

        D3D12_CLEAR_VALUE* clearValue = nullptr;
        D3D12_CLEAR_VALUE cv = {};
        if (info.Usage & TextureUsageFlags::DepthStencilAttachment)
        {
            cv.Format = ToDXGIFormat(info.Format);
            cv.DepthStencil = {1.0f, 0};
            clearValue = &cv;
        }
        else if (info.Usage & TextureUsageFlags::ColorAttachment)
        {
            cv.Format = desc.Format;
            cv.Color[0] = cv.Color[1] = cv.Color[2] = 0.0f;
            cv.Color[3] = 1.0f;
            clearValue = &cv;
        }

        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_STATES initialState = ToD3D12ResourceState(m_currentState);
        d3dDevice->GetAllocator()->CreateResource(
            &allocDesc, &desc, initialState,
            clearValue, &m_allocation, IID_PPV_ARGS(&m_resource));
    }

    D3D12Texture::D3D12Texture(const Ref<Device>& device, ComPtr<ID3D12Resource> resource,
                               const TextureCreateInfo& info)
        : m_device(device), m_resource(std::move(resource)), m_createInfo(info),
          m_ownsResource(false), m_currentState(ResourceState::Present)
    {
    }

    D3D12Texture::~D3D12Texture()
    {
        m_defaultView.reset();
        m_resource.Reset();
        if (m_allocation) { m_allocation->Release(); m_allocation = nullptr; }
    }

    Ref<CacaoTextureView> D3D12Texture::CreateView(const TextureViewDesc& desc)
    {
        auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(m_device);
        if (!d3dDevice || !d3dDevice->GetHandle())
            throw std::runtime_error("D3D12Texture::CreateView: device is null");
        return std::make_shared<D3D12TextureView>(shared_from_this(), desc, d3dDevice);
    }

    void D3D12Texture::CreateDefaultViewIfNeeded()
    {
        if (m_defaultView) return;
        TextureViewDesc desc{};
        desc.ViewType = m_createInfo.Type;
        desc.BaseMipLevel = 0;
        desc.MipLevelCount = m_createInfo.MipLevels;
        desc.BaseArrayLayer = 0;
        desc.ArrayLayerCount = m_createInfo.ArrayLayers;
        desc.Aspect = IsDepthStencil() ? AspectMask::DepthStencil : AspectMask::Color;
        m_defaultView = CreateView(desc);
    }

    D3D12TextureView::D3D12TextureView(const Ref<Texture>& texture, const TextureViewDesc& desc,
                                       const std::shared_ptr<D3D12Device>& d3dDevice)
        : m_texture(texture), m_desc(desc)
    {
        auto* d3dTex = static_cast<D3D12Texture*>(texture.get());
        auto* device = d3dDevice->GetHandle();
        auto usage = d3dTex->GetUsage();
        bool isDepth = d3dTex->IsDepthStencil();

        if (usage & TextureUsageFlags::Sampled)
        {
            m_srvHandle = d3dDevice->AllocateCbvSrvUav();
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            DXGI_FORMAT srvFormat = ToDXGIFormat(desc.FormatOverride != Format::UNDEFINED ? desc.FormatOverride : texture->GetFormat());
            if (isDepth)
            {
                if (srvFormat == DXGI_FORMAT_D32_FLOAT)       srvFormat = DXGI_FORMAT_R32_FLOAT;
                else if (srvFormat == DXGI_FORMAT_D24_UNORM_S8_UINT) srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                else if (srvFormat == DXGI_FORMAT_D16_UNORM)  srvFormat = DXGI_FORMAT_R16_UNORM;
                else if (srvFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            }
            srvDesc.Format = srvFormat;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MostDetailedMip = desc.BaseMipLevel;
            srvDesc.Texture2D.MipLevels = desc.MipLevelCount;
            device->CreateShaderResourceView(d3dTex->GetHandle(), &srvDesc, m_srvHandle);
            m_hasSRV = true;
        }

        if (!isDepth && (usage & TextureUsageFlags::ColorAttachment))
        {
            m_rtvHandle = d3dDevice->AllocateRTV();
            device->CreateRenderTargetView(d3dTex->GetHandle(), nullptr, m_rtvHandle);
            m_hasRTV = true;
        }

        if (isDepth && (usage & TextureUsageFlags::DepthStencilAttachment))
        {
            m_dsvHandle = d3dDevice->AllocateDSV();
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
            dsvDesc.Format = ToDXGIFormat(texture->GetFormat());
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
            device->CreateDepthStencilView(d3dTex->GetHandle(), &dsvDesc, m_dsvHandle);
            m_hasDSV = true;
        }

        if (usage & TextureUsageFlags::Storage)
        {
            m_uavHandle = d3dDevice->AllocateCbvSrvUav();
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = ToDXGIFormat(desc.FormatOverride != Format::UNDEFINED ? desc.FormatOverride : texture->GetFormat());
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = desc.BaseMipLevel;
            device->CreateUnorderedAccessView(d3dTex->GetHandle(), nullptr, &uavDesc, m_uavHandle);
            m_hasUAV = true;
        }
    }
}
