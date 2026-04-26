#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Texture.h"

#include <algorithm>

namespace luna::RHI {
namespace {

uint32_t resolveMipLevelCount(const D3D11Texture& texture, const TextureViewDesc& desc)
{
    const uint32_t available = texture.GetMipLevels() > desc.BaseMipLevel ? texture.GetMipLevels() - desc.BaseMipLevel : 1u;
    return desc.MipLevelCount == 0 ? available : std::min(desc.MipLevelCount, available);
}

uint32_t resolveArrayLayerCount(const D3D11Texture& texture, const TextureViewDesc& desc)
{
    const uint32_t available =
        texture.GetArrayLayers() > desc.BaseArrayLayer ? texture.GetArrayLayers() - desc.BaseArrayLayer : 1u;
    return desc.ArrayLayerCount == 0 ? available : std::min(desc.ArrayLayerCount, available);
}

DXGI_FORMAT resolveViewFormat(const D3D11Texture& texture, const TextureViewDesc& desc)
{
    return D3D11_ToDXGIFormat(desc.FormatOverride != Format::UNDEFINED ? desc.FormatOverride : texture.GetFormat());
}

} // namespace

D3D11TextureView::D3D11TextureView(Ref<D3D11Texture> texture, Ref<D3D11Device> device, TextureViewDesc desc)
    : m_texture(texture),
      m_desc(std::move(desc))
{
    if (!texture || !device || !device->GetNativeDevice()) {
        return;
    }

    auto* native_device = device->GetNativeDevice();
    const DXGI_FORMAT view_format = resolveViewFormat(*texture, m_desc);
    const UINT mip_count = static_cast<UINT>(resolveMipLevelCount(*texture, m_desc));
    const UINT array_layer_count = static_cast<UINT>(resolveArrayLayerCount(*texture, m_desc));

    if (texture->GetUsage() & TextureUsageFlags::Sampled) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = view_format;
        switch (m_desc.ViewType) {
            case TextureType::Texture2DArray:
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                srv_desc.Texture2DArray.MostDetailedMip = m_desc.BaseMipLevel;
                srv_desc.Texture2DArray.MipLevels = mip_count;
                srv_desc.Texture2DArray.FirstArraySlice = m_desc.BaseArrayLayer;
                srv_desc.Texture2DArray.ArraySize = array_layer_count;
                break;
            case TextureType::TextureCube:
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                srv_desc.TextureCube.MostDetailedMip = m_desc.BaseMipLevel;
                srv_desc.TextureCube.MipLevels = mip_count;
                break;
            case TextureType::TextureCubeArray:
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                srv_desc.TextureCubeArray.MostDetailedMip = m_desc.BaseMipLevel;
                srv_desc.TextureCubeArray.MipLevels = mip_count;
                srv_desc.TextureCubeArray.First2DArrayFace = m_desc.BaseArrayLayer;
                srv_desc.TextureCubeArray.NumCubes = std::max<UINT>(array_layer_count / 6u, 1u);
                break;
            case TextureType::Texture2D:
            default:
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MostDetailedMip = m_desc.BaseMipLevel;
                srv_desc.Texture2D.MipLevels = mip_count;
                break;
        }
        native_device->CreateShaderResourceView(texture->GetNativeTexture(), &srv_desc, &m_srv);
    }

    if (!texture->IsDepthStencil() && (texture->GetUsage() & TextureUsageFlags::ColorAttachment)) {
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
        rtv_desc.Format = view_format;
        if (m_desc.ViewType == TextureType::Texture2DArray || m_desc.ViewType == TextureType::TextureCube ||
            m_desc.ViewType == TextureType::TextureCubeArray) {
            rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtv_desc.Texture2DArray.MipSlice = m_desc.BaseMipLevel;
            rtv_desc.Texture2DArray.FirstArraySlice = m_desc.BaseArrayLayer;
            rtv_desc.Texture2DArray.ArraySize = array_layer_count;
        } else {
            rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtv_desc.Texture2D.MipSlice = m_desc.BaseMipLevel;
        }
        native_device->CreateRenderTargetView(texture->GetNativeTexture(), &rtv_desc, &m_rtv);
    }

    if (texture->IsDepthStencil() && (texture->GetUsage() & TextureUsageFlags::DepthStencilAttachment)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.Format = D3D11_ToDXGIFormat(texture->GetFormat());
        if (m_desc.ViewType == TextureType::Texture2DArray || m_desc.ViewType == TextureType::TextureCube ||
            m_desc.ViewType == TextureType::TextureCubeArray) {
            dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsv_desc.Texture2DArray.MipSlice = m_desc.BaseMipLevel;
            dsv_desc.Texture2DArray.FirstArraySlice = m_desc.BaseArrayLayer;
            dsv_desc.Texture2DArray.ArraySize = array_layer_count;
        } else {
            dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsv_desc.Texture2D.MipSlice = m_desc.BaseMipLevel;
        }
        native_device->CreateDepthStencilView(texture->GetNativeTexture(), &dsv_desc, &m_dsv);
    }

    if (texture->GetUsage() & TextureUsageFlags::Storage) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        uav_desc.Format = view_format;
        if (m_desc.ViewType == TextureType::Texture2DArray || m_desc.ViewType == TextureType::TextureCube ||
            m_desc.ViewType == TextureType::TextureCubeArray) {
            uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
            uav_desc.Texture2DArray.MipSlice = m_desc.BaseMipLevel;
            uav_desc.Texture2DArray.FirstArraySlice = m_desc.BaseArrayLayer;
            uav_desc.Texture2DArray.ArraySize = array_layer_count;
        } else {
            uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Texture2D.MipSlice = m_desc.BaseMipLevel;
        }
        native_device->CreateUnorderedAccessView(texture->GetNativeTexture(), &uav_desc, &m_uav);
    }
}

Ref<Texture> D3D11TextureView::GetTexture() const
{
    return m_texture.lock();
}

D3D11Texture::D3D11Texture(Ref<D3D11Device> device, const TextureCreateInfo& createInfo)
    : m_device(std::move(device)),
      m_width(createInfo.Width),
      m_height(createInfo.Height),
      m_depth(createInfo.Depth),
      m_mipLevels(createInfo.MipLevels),
      m_arrayLayers(createInfo.ArrayLayers),
      m_format(createInfo.Format),
      m_type(createInfo.Type),
      m_sampleCount(createInfo.SampleCount),
      m_usage(createInfo.Usage),
      m_currentState(createInfo.InitialState)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = m_mipLevels;
    desc.ArraySize = m_arrayLayers;
    desc.Format = D3D11_ToDXGIFormat(m_format);
    desc.SampleDesc.Count = static_cast<UINT>(m_sampleCount);
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    if (m_type == TextureType::TextureCube || m_type == TextureType::TextureCubeArray) {
        desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    if (m_usage & TextureUsageFlags::Sampled) {
        desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (m_usage & TextureUsageFlags::ColorAttachment) {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }
    if (m_usage & TextureUsageFlags::DepthStencilAttachment) {
        desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
    }
    if (m_usage & TextureUsageFlags::Storage) {
        desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    D3D11_SUBRESOURCE_DATA initData{};
    D3D11_SUBRESOURCE_DATA* pInit = nullptr;
    if (createInfo.InitialData) {
        initData.pSysMem = createInfo.InitialData;
        uint32_t bpp = 4;
        switch (m_format) {
            case Format::R8_UNORM:
                bpp = 1;
                break;
            case Format::RG8_UNORM:
                bpp = 2;
                break;
            case Format::R16_FLOAT:
                bpp = 2;
                break;
            case Format::RG16_FLOAT:
                bpp = 4;
                break;
            case Format::RGBA16_FLOAT:
                bpp = 8;
                break;
            case Format::R32_FLOAT:
                bpp = 4;
                break;
            case Format::RG32_FLOAT:
                bpp = 8;
                break;
            case Format::RGB32_FLOAT:
                bpp = 12;
                break;
            case Format::RGBA32_FLOAT:
                bpp = 16;
                break;
            default:
                bpp = 4;
                break;
        }
        initData.SysMemPitch = m_width * bpp;
        pInit = &initData;
    }

    m_device->GetNativeDevice()->CreateTexture2D(&desc, pInit, &m_texture);
    if (m_texture) {
        CreateViews();
    }
}

D3D11Texture::D3D11Texture(Ref<D3D11Device> device, ComPtr<ID3D11Texture2D> existingTexture, Format format)
    : m_device(std::move(device)),
      m_texture(std::move(existingTexture)),
      m_format(format)
{
    D3D11_TEXTURE2D_DESC desc;
    m_texture->GetDesc(&desc);
    m_width = desc.Width;
    m_height = desc.Height;
    m_mipLevels = desc.MipLevels;
    m_arrayLayers = desc.ArraySize;
    m_currentState = ResourceState::Present;
    if (desc.BindFlags & D3D11_BIND_RENDER_TARGET) {
        m_usage = m_usage | TextureUsageFlags::ColorAttachment;
    }
    if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
        m_usage = m_usage | TextureUsageFlags::Sampled;
    }
    if (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
        m_usage = m_usage | TextureUsageFlags::DepthStencilAttachment;
    }
    if (m_usage == TextureUsageFlags{}) {
        m_usage = TextureUsageFlags::ColorAttachment;
    }
    CreateViews();
}

void D3D11Texture::CreateViews()
{
    auto* dev = m_device->GetNativeDevice();

    if (m_usage & TextureUsageFlags::Sampled) {
        dev->CreateShaderResourceView(m_texture.Get(), nullptr, &m_srv);
    }
    if (m_usage & TextureUsageFlags::ColorAttachment) {
        dev->CreateRenderTargetView(m_texture.Get(), nullptr, &m_rtv);
    }
    if (m_usage & TextureUsageFlags::DepthStencilAttachment) {
        dev->CreateDepthStencilView(m_texture.Get(), nullptr, &m_dsv);
    }
    if (m_usage & TextureUsageFlags::Storage) {
        dev->CreateUnorderedAccessView(m_texture.Get(), nullptr, &m_uav);
    }
}

Ref<TextureView> D3D11Texture::CreateView(const TextureViewDesc& desc)
{
    return CreateRef<D3D11TextureView>(std::static_pointer_cast<D3D11Texture>(shared_from_this()), m_device, desc);
}

Ref<TextureView> D3D11Texture::GetDefaultView()
{
    CreateDefaultViewIfNeeded();
    return m_defaultView;
}

void D3D11Texture::CreateDefaultViewIfNeeded()
{
    if (m_defaultView) {
        return;
    }

    TextureViewDesc desc{};
    desc.ViewType = m_type;
    desc.FormatOverride = Format::UNDEFINED;
    desc.BaseMipLevel = 0;
    desc.MipLevelCount = m_mipLevels;
    desc.BaseArrayLayer = 0;
    desc.ArrayLayerCount = m_arrayLayers;
    desc.Aspect = IsDepthStencil() ? AspectMask::DepthStencil : AspectMask::Color;

    m_defaultView = CreateView(desc);
}
} // namespace luna::RHI
