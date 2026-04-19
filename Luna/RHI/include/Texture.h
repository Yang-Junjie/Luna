#ifndef LUNA_RHI_TEXTURE_H
#define LUNA_RHI_TEXTURE_H
#include "Barrier.h"

namespace luna::RHI {
enum class TextureType {
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    Texture1DArray,
    Texture2DArray,
    TextureCubeArray
};
enum class TextureUsageFlags : uint32_t {
    None = 0,
    TransferSrc = 1 << 0,
    TransferDst = 1 << 1,
    Sampled = 1 << 2,
    Storage = 1 << 3,
    ColorAttachment = 1 << 4,
    DepthStencilAttachment = 1 << 5,
    TransientAttachment = 1 << 6,
    InputAttachment = 1 << 7
};

inline TextureUsageFlags operator|(TextureUsageFlags a, TextureUsageFlags b)
{
    return static_cast<TextureUsageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline TextureUsageFlags& operator|=(TextureUsageFlags& a, TextureUsageFlags b)
{
    a = a | b;
    return a;
}

inline bool operator&(TextureUsageFlags a, TextureUsageFlags b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}
enum class SampleCount : uint32_t {
    Count1 = 1,
    Count2 = 2,
    Count4 = 4,
    Count8 = 8,
    Count16 = 16,
    Count32 = 32,
    Count64 = 64
};

struct TextureCreateInfo {
    TextureType Type = TextureType::Texture2D;
    uint32_t Width = 1;
    uint32_t Height = 1;
    uint32_t Depth = 1;
    uint32_t ArrayLayers = 1;
    uint32_t MipLevels = 1;
    Format Format = Format::RGBA8_UNORM;
    TextureUsageFlags Usage = TextureUsageFlags::Sampled | TextureUsageFlags::TransferDst;
    ResourceState InitialState = ResourceState::Undefined;
    SampleCount SampleCount = SampleCount::Count1;
    std::string Name;
    void* InitialData = nullptr;
};
enum class AspectMask : uint32_t {
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    DepthStencil = Depth | Stencil
};

inline AspectMask operator|(AspectMask a, AspectMask b)
{
    return static_cast<AspectMask>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline AspectMask& operator|=(AspectMask& a, AspectMask b)
{
    a = a | b;
    return a;
}

inline bool operator!(AspectMask a)
{
    return static_cast<uint32_t>(a) == 0;
}

inline bool operator&(AspectMask a, AspectMask b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct TextureViewDesc {
    TextureType ViewType = TextureType::Texture2D;
    Format FormatOverride = Format::UNDEFINED;
    uint32_t BaseMipLevel = 0;
    uint32_t MipLevelCount = 1;
    uint32_t BaseArrayLayer = 0;
    uint32_t ArrayLayerCount = 1;
    AspectMask Aspect = AspectMask::Color;
    std::string Name;
};
class LUNA_RHI_API Texture;

class LUNA_RHI_API TextureView : public std::enable_shared_from_this<TextureView> {
public:
    // TextureView must not strongly own its parent texture. Backends should store
    // native view handles directly and use a weak back-reference when needed.
    virtual Ref<Texture> GetTexture() const = 0;
    virtual const TextureViewDesc& GetDesc() const = 0;
    virtual ~TextureView() = default;
};

class LUNA_RHI_API Texture : public std::enable_shared_from_this<Texture> {
public:
    virtual ~Texture() = default;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual uint32_t GetDepth() const = 0;
    virtual uint32_t GetMipLevels() const = 0;
    virtual uint32_t GetArrayLayers() const = 0;
    virtual Format GetFormat() const = 0;
    virtual TextureType GetType() const = 0;
    virtual SampleCount GetSampleCount() const = 0;
    virtual TextureUsageFlags GetUsage() const = 0;
    virtual ResourceState GetCurrentState() const = 0;
    virtual Ref<TextureView> CreateView(const TextureViewDesc& desc) = 0;
    // GetDefaultView() should lazily create and return the backend default view.
    virtual Ref<TextureView> GetDefaultView() = 0;
    virtual void CreateDefaultViewIfNeeded() = 0;

    bool IsDepthStencil() const
    {
        return IsDepthFormat(GetFormat()) || IsStencilFormat(GetFormat());
    }

    bool HasDepth() const
    {
        return IsDepthFormat(GetFormat());
    }

    bool HasStencil() const
    {
        return IsStencilFormat(GetFormat());
    }

protected:
    static bool IsDepthFormat(Format format)
    {
        return format == Format::D16_UNORM || format == Format::D32_FLOAT || format == Format::D32F ||
               format == Format::D24_UNORM_S8_UINT || format == Format::D24S8 || format == Format::D32_FLOAT_S8_UINT;
    }

    static bool IsStencilFormat(Format format)
    {
        return format == Format::D24_UNORM_S8_UINT || format == Format::D24S8 || format == Format::D32_FLOAT_S8_UINT ||
               format == Format::S8_UINT;
    }
};
} // namespace luna::RHI
#endif
