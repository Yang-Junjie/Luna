#include "Impls/WebGPU/WGPUTexture.h"

namespace Cacao
{
    // --- WGPUTextureViewImpl ---

    WGPUTextureViewImpl::WGPUTextureViewImpl(::WGPUTextureView view, const Ref<Texture>& texture,
                                              const TextureViewDesc& desc)
        : m_view(view), m_texture(texture), m_desc(desc)
    {
    }

    WGPUTextureViewImpl::~WGPUTextureViewImpl()
    {
        if (m_view)
        {
            wgpuTextureViewRelease(m_view);
            m_view = nullptr;
        }
    }

    Ref<Texture> WGPUTextureViewImpl::GetTexture() const { return m_texture; }
    const TextureViewDesc& WGPUTextureViewImpl::GetDesc() const { return m_desc; }

    // --- WGPUTextureImpl ---

    WGPUTextureImpl::WGPUTextureImpl(::WGPUDevice device, const TextureCreateInfo& info)
        : m_wgpuDevice(device), m_createInfo(info), m_ownsTexture(true)
    {
        WGPUTextureDescriptor desc = {};
        desc.dimension = ToWGPUTextureDimension(info.Type);
        desc.size = {info.Width, info.Height, info.Depth};
        desc.mipLevelCount = info.MipLevels;
        desc.sampleCount = static_cast<uint32_t>(info.SampleCount);
        desc.format = ToWGPUFormat(info.Format);
        desc.usage = ToWGPUTextureUsage(info.Usage);
        if (!info.Name.empty())
            desc.label = info.Name.c_str();

        m_texture = wgpuDeviceCreateTexture(device, &desc);
        m_currentState = info.InitialState;
    }

    WGPUTextureImpl::WGPUTextureImpl(::WGPUTexture texture, const TextureCreateInfo& info)
        : m_texture(texture), m_createInfo(info), m_ownsTexture(false)
    {
        m_currentState = info.InitialState;
    }

    WGPUTextureImpl::~WGPUTextureImpl()
    {
        m_defaultView.reset();
        if (m_texture && m_ownsTexture)
        {
            wgpuTextureRelease(m_texture);
            m_texture = nullptr;
        }
    }

    uint32_t WGPUTextureImpl::GetWidth() const { return m_createInfo.Width; }
    uint32_t WGPUTextureImpl::GetHeight() const { return m_createInfo.Height; }
    uint32_t WGPUTextureImpl::GetDepth() const { return m_createInfo.Depth; }
    uint32_t WGPUTextureImpl::GetMipLevels() const { return m_createInfo.MipLevels; }
    uint32_t WGPUTextureImpl::GetArrayLayers() const { return m_createInfo.ArrayLayers; }
    Format WGPUTextureImpl::GetFormat() const { return m_createInfo.Format; }
    TextureType WGPUTextureImpl::GetType() const { return m_createInfo.Type; }
    SampleCount WGPUTextureImpl::GetSampleCount() const { return m_createInfo.SampleCount; }
    TextureUsageFlags WGPUTextureImpl::GetUsage() const { return m_createInfo.Usage; }
    ResourceState WGPUTextureImpl::GetCurrentState() const { return m_currentState; }

    Ref<CacaoTextureView> WGPUTextureImpl::CreateView(const TextureViewDesc& desc)
    {
        WGPUTextureViewDescriptor viewDesc = {};
        viewDesc.format = (desc.FormatOverride != Format::UNDEFINED)
                          ? ToWGPUFormat(desc.FormatOverride)
                          : ToWGPUFormat(m_createInfo.Format);
        viewDesc.dimension = ToWGPUTextureViewDimension(desc.ViewType);
        viewDesc.baseMipLevel = desc.BaseMipLevel;
        viewDesc.mipLevelCount = desc.MipLevelCount;
        viewDesc.baseArrayLayer = desc.BaseArrayLayer;
        viewDesc.arrayLayerCount = desc.ArrayLayerCount;

        if (desc.Aspect & AspectMask::Depth)
            viewDesc.aspect = WGPUTextureAspect_DepthOnly;
        else if (desc.Aspect & AspectMask::Stencil)
            viewDesc.aspect = WGPUTextureAspect_StencilOnly;
        else
            viewDesc.aspect = WGPUTextureAspect_All;

        if (!desc.Name.empty())
            viewDesc.label = desc.Name.c_str();

        ::WGPUTextureView nativeView = wgpuTextureCreateView(m_texture, &viewDesc);
        return std::make_shared<WGPUTextureViewImpl>(nativeView, shared_from_this(), desc);
    }

    Ref<CacaoTextureView> WGPUTextureImpl::GetDefaultView()
    {
        return m_defaultView;
    }

    void WGPUTextureImpl::CreateDefaultViewIfNeeded()
    {
        if (m_defaultView)
            return;

        TextureViewDesc desc;
        desc.ViewType = m_createInfo.Type;
        desc.FormatOverride = Format::UNDEFINED;
        desc.BaseMipLevel = 0;
        desc.MipLevelCount = m_createInfo.MipLevels;
        desc.BaseArrayLayer = 0;
        desc.ArrayLayerCount = m_createInfo.ArrayLayers;
        desc.Aspect = IsDepthFormat(m_createInfo.Format) ? AspectMask::Depth : AspectMask::Color;

        m_defaultView = CreateView(desc);
    }
}
