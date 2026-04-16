#include "Impls/OpenGL/GLTexture.h"

namespace Cacao {
// --- GLTextureView ---
GLTextureView::GLTextureView(const Ref<Texture>& texture, const TextureViewDesc& desc)
    : m_texture(std::dynamic_pointer_cast<GLTexture>(texture)),
      m_desc(desc)
{}

Ref<GLTextureView> GLTextureView::Create(const Ref<Texture>& texture, const TextureViewDesc& desc)
{
    return std::make_shared<GLTextureView>(texture, desc);
}

Ref<Texture> GLTextureView::GetTexture() const
{
    return m_texture;
}

const TextureViewDesc& GLTextureView::GetDesc() const
{
    return m_desc;
}

// --- GLTexture ---
GLTexture::GLTexture(const Ref<Device>& device, const TextureCreateInfo& info)
    : m_device(device),
      m_createInfo(info)
{
    m_target = (info.Type == TextureType::Texture3D) ? GL_TEXTURE_3D
               : (info.ArrayLayers > 1)              ? GL_TEXTURE_2D_ARRAY
                                                     : GL_TEXTURE_2D;

    glGenTextures(1, &m_texture);
    glBindTexture(m_target, m_texture);

    auto glFmt = FormatToGL(info.Format);

    if (m_target == GL_TEXTURE_2D) {
        glTexStorage2D(m_target, info.MipLevels, glFmt.internalFormat, info.Width, info.Height);
    } else if (m_target == GL_TEXTURE_3D || m_target == GL_TEXTURE_2D_ARRAY) {
        uint32_t depth = (m_target == GL_TEXTURE_3D) ? info.Depth : info.ArrayLayers;
        glTexStorage3D(m_target, info.MipLevels, glFmt.internalFormat, info.Width, info.Height, depth);
    }

    glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(m_target, 0);
}

Ref<GLTexture> GLTexture::Create(const Ref<Device>& device, const TextureCreateInfo& info)
{
    return std::make_shared<GLTexture>(device, info);
}

GLTexture::~GLTexture()
{
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
    }
}

uint32_t GLTexture::GetWidth() const
{
    return m_createInfo.Width;
}

uint32_t GLTexture::GetHeight() const
{
    return m_createInfo.Height;
}

uint32_t GLTexture::GetDepth() const
{
    return m_createInfo.Depth;
}

uint32_t GLTexture::GetMipLevels() const
{
    return m_createInfo.MipLevels;
}

uint32_t GLTexture::GetArrayLayers() const
{
    return m_createInfo.ArrayLayers;
}

Format GLTexture::GetFormat() const
{
    return m_createInfo.Format;
}

TextureType GLTexture::GetType() const
{
    return m_createInfo.Type;
}

SampleCount GLTexture::GetSampleCount() const
{
    return m_createInfo.SampleCount;
}

TextureUsageFlags GLTexture::GetUsage() const
{
    return m_createInfo.Usage;
}

ResourceState GLTexture::GetCurrentState() const
{
    return ResourceState::Undefined;
}

Ref<CacaoTextureView> GLTexture::CreateView(const TextureViewDesc& desc)
{
    return GLTextureView::Create(shared_from_this(), desc);
}

Ref<CacaoTextureView> GLTexture::GetDefaultView()
{
    CreateDefaultViewIfNeeded();
    return m_defaultView;
}

void GLTexture::CreateDefaultViewIfNeeded()
{
    if (!m_defaultView) {
        TextureViewDesc desc{};
        m_defaultView = GLTextureView::Create(shared_from_this(), desc);
    }
}

void GLTexture::UploadData(
    const void* data, uint32_t width, uint32_t height, uint32_t xOffset, uint32_t yOffset, uint32_t mipLevel)
{
    auto glFmt = FormatToGL(m_createInfo.Format);
    glBindTexture(m_target, m_texture);
    glTexSubImage2D(m_target, mipLevel, xOffset, yOffset, width, height, glFmt.format, glFmt.type, data);
    glBindTexture(m_target, 0);
}

void GLTexture::GenerateMipmaps()
{
    glBindTexture(m_target, m_texture);
    glGenerateMipmap(m_target);
    glBindTexture(m_target, 0);
}
} // namespace Cacao
