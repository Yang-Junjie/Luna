#ifdef __APPLE__
#import <Metal/Metal.h>
#include "Impls/Metal/MTLTexture.h"

namespace luna::RHI
{
    static MTLPixelFormat ToMTLPixelFormat(Format format)
    {
        switch (format)
        {
        case Format::RGBA8_UNORM:  return MTLPixelFormatRGBA8Unorm;
        case Format::RGBA8_SRGB:   return MTLPixelFormatRGBA8Unorm_sRGB;
        case Format::BGRA8_UNORM:  return MTLPixelFormatBGRA8Unorm;
        case Format::BGRA8_SRGB:   return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::R8_UNORM:     return MTLPixelFormatR8Unorm;
        case Format::RG8_UNORM:    return MTLPixelFormatRG8Unorm;
        case Format::R16_FLOAT:    return MTLPixelFormatR16Float;
        case Format::RG16_FLOAT:   return MTLPixelFormatRG16Float;
        case Format::RGBA16_FLOAT: return MTLPixelFormatRGBA16Float;
        case Format::R32_FLOAT:    return MTLPixelFormatR32Float;
        case Format::RG32_FLOAT:   return MTLPixelFormatRG32Float;
        case Format::RGBA32_FLOAT: return MTLPixelFormatRGBA32Float;
        case Format::D32F:         return MTLPixelFormatDepth32Float;
        case Format::D24S8:        return MTLPixelFormatDepth24Unorm_Stencil8;
        default:                   return MTLPixelFormatRGBA8Unorm;
        }
    }

    static MTLTextureType ToMTLTextureType(TextureType type)
    {
        switch (type)
        {
        case TextureType::Texture1D:        return MTLTextureType1D;
        case TextureType::Texture1DArray:   return MTLTextureType1DArray;
        case TextureType::Texture2D:        return MTLTextureType2D;
        case TextureType::Texture2DArray:   return MTLTextureType2DArray;
        case TextureType::Texture3D:        return MTLTextureType3D;
        case TextureType::TextureCube:      return MTLTextureTypeCube;
        case TextureType::TextureCubeArray: return MTLTextureTypeCubeArray;
        default:                            return MTLTextureType2D;
        }
    }

    static MTLTextureUsage ToMTLTextureUsage(TextureUsageFlags flags)
    {
        MTLTextureUsage result = 0;
        if (flags & TextureUsageFlags::Sampled)               result |= MTLTextureUsageShaderRead;
        if (flags & TextureUsageFlags::Storage)               result |= MTLTextureUsageShaderWrite;
        if (flags & TextureUsageFlags::ColorAttachment)       result |= MTLTextureUsageRenderTarget;
        if (flags & TextureUsageFlags::DepthStencilAttachment) result |= MTLTextureUsageRenderTarget;
        if (result == 0) result = MTLTextureUsageShaderRead;
        return result;
    }

    // --- MTLTextureViewImpl ---

    MTLTextureViewImpl::MTLTextureViewImpl(id textureView, const Ref<Texture>& texture, const TextureViewDesc& desc)
        : m_view(textureView), m_texture(texture), m_desc(desc)
    {
    }

    MTLTextureViewImpl::~MTLTextureViewImpl()
    {
        m_view = nil;
    }

    Ref<Texture> MTLTextureViewImpl::GetTexture() const { return m_texture.lock(); }
    const TextureViewDesc& MTLTextureViewImpl::GetDesc() const { return m_desc; }

    // --- MTLTextureImpl ---

    MTLTextureImpl::MTLTextureImpl(id device, const TextureCreateInfo& info)
        : m_createInfo(info), m_ownsTexture(true)
    {
        id<MTLDevice> mtlDevice = (id<MTLDevice>)device;

        MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
        desc.textureType = ToMTLTextureType(info.Type);
        desc.pixelFormat = ToMTLPixelFormat(info.Format);
        desc.width = info.Width;
        desc.height = info.Height;
        desc.depth = info.Depth;
        desc.mipmapLevelCount = info.MipLevels;
        desc.arrayLength = info.ArrayLayers;
        desc.sampleCount = static_cast<NSUInteger>(info.SampleCount);
        desc.usage = ToMTLTextureUsage(info.Usage);
        desc.storageMode = MTLStorageModePrivate;

        if (info.Usage & TextureUsageFlags::TransferDst)
            desc.storageMode = MTLStorageModeShared;

        m_texture = [mtlDevice newTextureWithDescriptor:desc];

        if (!info.Name.empty() && m_texture)
        {
            id<MTLTexture> tex = (id<MTLTexture>)m_texture;
            [tex setLabel:[NSString stringWithUTF8String:info.Name.c_str()]];
        }

        m_currentState = info.InitialState;
    }

    MTLTextureImpl::MTLTextureImpl(id texture, const TextureCreateInfo& info, bool ownsTexture)
        : m_texture(texture), m_createInfo(info), m_ownsTexture(ownsTexture)
    {
        m_currentState = info.InitialState;
    }

    MTLTextureImpl::~MTLTextureImpl()
    {
        m_defaultView.reset();
        if (m_ownsTexture)
            m_texture = nil;
    }

    Ref<TextureView> MTLTextureImpl::CreateView(const TextureViewDesc& desc)
    {
        id<MTLTexture> baseTex = (id<MTLTexture>)m_texture;
        MTLPixelFormat fmt = (desc.FormatOverride != Format::UNDEFINED)
                             ? ToMTLPixelFormat(desc.FormatOverride)
                             : baseTex.pixelFormat;
        MTLTextureType viewType = ToMTLTextureType(desc.ViewType);

        NSRange levelRange = NSMakeRange(desc.BaseMipLevel, desc.MipLevelCount);
        NSRange sliceRange = NSMakeRange(desc.BaseArrayLayer, desc.ArrayLayerCount);

        id<MTLTexture> viewTex = [baseTex newTextureViewWithPixelFormat:fmt
                                                           textureType:viewType
                                                                levels:levelRange
                                                                slices:sliceRange];
        return std::make_shared<MTLTextureViewImpl>((id)viewTex, shared_from_this(), desc);
    }

    Ref<TextureView> MTLTextureImpl::GetDefaultView()
    {
        CreateDefaultViewIfNeeded();
        return m_defaultView;
    }

    void MTLTextureImpl::CreateDefaultViewIfNeeded()
    {
        if (m_defaultView) return;

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
#endif // __APPLE__
