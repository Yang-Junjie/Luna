#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Texture.h"

namespace Cacao {
Ref<Texture> D3D11TextureView::GetTexture() const
{
    return m_texture;
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

Ref<CacaoTextureView> D3D11Texture::CreateView(const TextureViewDesc& desc)
{
    return m_defaultView;
}

Ref<CacaoTextureView> D3D11Texture::GetDefaultView()
{
    return m_defaultView;
}

void D3D11Texture::CreateDefaultViewIfNeeded()
{
    if (m_defaultView) {
        return;
    }
    m_defaultView = CreateRef<D3D11TextureView>(std::static_pointer_cast<D3D11Texture>(shared_from_this()));
}
} // namespace Cacao
