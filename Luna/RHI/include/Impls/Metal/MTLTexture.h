#ifndef LUNA_RHI_MTLTEXTURE_H
#define LUNA_RHI_MTLTEXTURE_H
#ifdef __APPLE__
#include "MTLCommon.h"
#include "Texture.h"

#include <memory>

namespace luna::RHI {
class MTLTextureImpl;

class LUNA_RHI_API MTLTextureViewImpl : public TextureView {
public:
    MTLTextureViewImpl(id textureView, const Ref<Texture>& texture, const TextureViewDesc& desc);
    ~MTLTextureViewImpl() override;

    Ref<Texture> GetTexture() const override;
    const TextureViewDesc& GetDesc() const override;

    id GetHandle() const
    {
        return m_view;
    }

private:
    id m_view = nullptr; // id<MTLTexture> (texture view)
    std::weak_ptr<Texture> m_texture;
    TextureViewDesc m_desc;
};

class LUNA_RHI_API MTLTextureImpl final : public Texture {
public:
    MTLTextureImpl(id device, const TextureCreateInfo& info);
    MTLTextureImpl(id texture, const TextureCreateInfo& info, bool ownsTexture);
    ~MTLTextureImpl() override;

    uint32_t GetWidth() const override
    {
        return m_createInfo.Width;
    }

    uint32_t GetHeight() const override
    {
        return m_createInfo.Height;
    }

    uint32_t GetDepth() const override
    {
        return m_createInfo.Depth;
    }

    Format GetFormat() const override
    {
        return m_createInfo.Format;
    }

    uint32_t GetMipLevels() const override
    {
        return m_createInfo.MipLevels;
    }

    uint32_t GetArrayLayers() const override
    {
        return m_createInfo.ArrayLayers;
    }

    TextureType GetType() const override
    {
        return m_createInfo.Type;
    }

    SampleCount GetSampleCount() const override
    {
        return m_createInfo.SampleCount;
    }

    TextureUsageFlags GetUsage() const override
    {
        return m_createInfo.Usage;
    }

    ResourceState GetCurrentState() const override
    {
        return m_currentState;
    }

    Ref<TextureView> CreateView(const TextureViewDesc& desc) override;
    Ref<TextureView> GetDefaultView() override;
    void CreateDefaultViewIfNeeded() override;

    id GetHandle() const
    {
        return m_texture;
    }

private:
    TextureCreateInfo m_createInfo;
    id m_texture = nullptr; // id<MTLTexture>
    Ref<TextureView> m_defaultView;
    ResourceState m_currentState = ResourceState::Undefined;
    bool m_ownsTexture = true;
};
} // namespace luna::RHI
#endif // __APPLE__
#endif
