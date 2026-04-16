#ifndef CACAO_WGPU_TEXTURE_H
#define CACAO_WGPU_TEXTURE_H

#include "Texture.h"
#include "WGPUCommon.h"

namespace Cacao {
class WGPUTextureImpl;

class CACAO_API WGPUTextureViewImpl : public CacaoTextureView {
private:
    ::WGPUTextureView m_view = nullptr;
    Ref<Texture> m_texture;
    TextureViewDesc m_desc;

    friend class WGPUCommandBufferEncoder;
    friend class WGPUDescriptorSet;

public:
    WGPUTextureViewImpl(::WGPUTextureView view, const Ref<Texture>& texture, const TextureViewDesc& desc);
    ~WGPUTextureViewImpl() override;

    Ref<Texture> GetTexture() const override;
    const TextureViewDesc& GetDesc() const override;

    ::WGPUTextureView GetNativeView() const
    {
        return m_view;
    }
};

class CACAO_API WGPUTextureImpl final : public Texture {
private:
    ::WGPUTexture m_texture = nullptr;
    ::WGPUDevice m_wgpuDevice = nullptr;
    TextureCreateInfo m_createInfo;
    Ref<CacaoTextureView> m_defaultView;
    ResourceState m_currentState = ResourceState::Undefined;
    bool m_ownsTexture = true;

    friend class WGPUSwapchain;
    friend class WGPUCommandBufferEncoder;
    friend class WGPUDescriptorSet;

public:
    WGPUTextureImpl(::WGPUDevice device, const TextureCreateInfo& info);
    WGPUTextureImpl(::WGPUTexture texture, const TextureCreateInfo& info);
    ~WGPUTextureImpl() override;

    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    uint32_t GetDepth() const override;
    uint32_t GetMipLevels() const override;
    uint32_t GetArrayLayers() const override;
    Format GetFormat() const override;
    TextureType GetType() const override;
    SampleCount GetSampleCount() const override;
    TextureUsageFlags GetUsage() const override;
    ResourceState GetCurrentState() const override;
    Ref<CacaoTextureView> CreateView(const TextureViewDesc& desc) override;
    Ref<CacaoTextureView> GetDefaultView() override;
    void CreateDefaultViewIfNeeded() override;

    ::WGPUTexture GetNativeTexture() const
    {
        return m_texture;
    }
};
} // namespace Cacao

#endif
