#ifndef CACAO_D3D11TEXTURE_H
#define CACAO_D3D11TEXTURE_H
#include "D3D11Common.h"

#include <Texture.h>

namespace Cacao {
class D3D11Device;
class D3D11Texture;

class CACAO_API D3D11TextureView : public CacaoTextureView {
public:
    D3D11TextureView(Ref<D3D11Texture> texture)
        : m_texture(std::move(texture))
    {}

    Ref<Texture> GetTexture() const override;

    const TextureViewDesc& GetDesc() const override
    {
        return m_desc;
    }

private:
    Ref<D3D11Texture> m_texture;
    TextureViewDesc m_desc;
};

class CACAO_API D3D11Texture : public Texture {
public:
    D3D11Texture(Ref<D3D11Device> device, const TextureCreateInfo& createInfo);
    D3D11Texture(Ref<D3D11Device> device, ComPtr<ID3D11Texture2D> existingTexture, Format format);

    uint32_t GetWidth() const override
    {
        return m_width;
    }

    uint32_t GetHeight() const override
    {
        return m_height;
    }

    uint32_t GetDepth() const override
    {
        return m_depth;
    }

    uint32_t GetMipLevels() const override
    {
        return m_mipLevels;
    }

    uint32_t GetArrayLayers() const override
    {
        return m_arrayLayers;
    }

    Format GetFormat() const override
    {
        return m_format;
    }

    TextureType GetType() const override
    {
        return m_type;
    }

    SampleCount GetSampleCount() const override
    {
        return m_sampleCount;
    }

    TextureUsageFlags GetUsage() const override
    {
        return m_usage;
    }

    ResourceState GetCurrentState() const override
    {
        return m_currentState;
    }

    Ref<CacaoTextureView> CreateView(const TextureViewDesc& desc) override;
    Ref<CacaoTextureView> GetDefaultView() override;
    void CreateDefaultViewIfNeeded() override;

    ID3D11Texture2D* GetNativeTexture() const
    {
        return m_texture.Get();
    }

    ID3D11ShaderResourceView* GetSRV() const
    {
        return m_srv.Get();
    }

    ID3D11RenderTargetView* GetRTV() const
    {
        return m_rtv.Get();
    }

    ID3D11DepthStencilView* GetDSV() const
    {
        return m_dsv.Get();
    }

    ID3D11UnorderedAccessView* GetUAV() const
    {
        return m_uav.Get();
    }

private:
    void CreateViews();

    Ref<D3D11Device> m_device;
    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11DepthStencilView> m_dsv;
    ComPtr<ID3D11UnorderedAccessView> m_uav;
    Ref<CacaoTextureView> m_defaultView;

    uint32_t m_width = 1, m_height = 1, m_depth = 1;
    uint32_t m_mipLevels = 1, m_arrayLayers = 1;
    Format m_format = Format::RGBA8_UNORM;
    TextureType m_type = TextureType::Texture2D;
    SampleCount m_sampleCount = SampleCount::Count1;
    TextureUsageFlags m_usage = TextureUsageFlags::Sampled;
    ResourceState m_currentState = ResourceState::Undefined;
};
} // namespace Cacao
#endif
