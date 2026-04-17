#ifndef LUNA_RHI_GLTEXTURE_H
#define LUNA_RHI_GLTEXTURE_H
#include "GLCommon.h"
#include "Texture.h"

namespace luna::RHI {
class Device;
class GLTexture;

class LUNA_RHI_API GLTextureView : public TextureView {
public:
    GLTextureView(const Ref<Texture>& texture, const TextureViewDesc& desc);
    static Ref<GLTextureView> Create(const Ref<Texture>& texture, const TextureViewDesc& desc);
    Ref<Texture> GetTexture() const override;
    const TextureViewDesc& GetDesc() const override;

private:
    Ref<GLTexture> m_texture;
    TextureViewDesc m_desc;
};

class LUNA_RHI_API GLTexture final : public Texture {
public:
    GLTexture(const Ref<Device>& device, const TextureCreateInfo& info);
    static Ref<GLTexture> Create(const Ref<Device>& device, const TextureCreateInfo& info);
    ~GLTexture() override;

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
    Ref<TextureView> CreateView(const TextureViewDesc& desc) override;
    Ref<TextureView> GetDefaultView() override;
    void CreateDefaultViewIfNeeded() override;

    GLuint GetHandle() const
    {
        return m_texture;
    }

    GLenum GetTarget() const
    {
        return m_target;
    }

    void UploadData(const void* data,
                    uint32_t width,
                    uint32_t height,
                    uint32_t xOffset = 0,
                    uint32_t yOffset = 0,
                    uint32_t mipLevel = 0);
    void GenerateMipmaps();

private:
    GLuint m_texture = 0;
    GLenum m_target = GL_TEXTURE_2D;
    Ref<Device> m_device;
    TextureCreateInfo m_createInfo;
    Ref<GLTextureView> m_defaultView;
};
} // namespace luna::RHI

#endif
