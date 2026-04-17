#ifndef LUNA_RHI_BARRIER_H
#define LUNA_RHI_BARRIER_H
#include "Core.h"

#include <vector>

namespace luna::RHI {
class Buffer;
class Texture;

enum class ResourceState : uint32_t {
    Undefined = 0,
    Common = 1 << 0,
    VertexBuffer = 1 << 1,
    IndexBuffer = 1 << 2,
    UniformBuffer = 1 << 3,
    RenderTarget = 1 << 4,
    UnorderedAccess = 1 << 5,
    DepthWrite = 1 << 6,
    DepthRead = 1 << 7,
    ShaderRead = 1 << 8,
    CopySource = 1 << 9,
    CopyDest = 1 << 10,
    Present = 1 << 11,
    IndirectArgument = 1 << 12,
    HostRead = 1 << 13,
    HostWrite = 1 << 14,
    General = 1 << 15,
};

inline ResourceState operator|(ResourceState a, ResourceState b)
{
    return static_cast<ResourceState>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(ResourceState a, ResourceState b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

enum class SyncScope : uint32_t {
    None = 0,
    VertexStage = 1 << 0,
    FragmentStage = 1 << 1,
    ComputeStage = 1 << 2,
    TransferStage = 1 << 3,
    HostStage = 1 << 4,
    AllGraphics = 1 << 5,
    AllCommands = 1 << 6,
};

inline SyncScope operator|(SyncScope a, SyncScope b)
{
    return static_cast<SyncScope>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// Legacy aliases for backward compatibility
using PipelineStage = SyncScope;
using AccessFlags = ResourceState;
using ImageLayout = ResourceState;

enum class ImageAspectFlags : uint32_t {
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    Metadata = 1 << 3,
    Plane0 = 1 << 4,
    Plane1 = 1 << 5,
    Plane2 = 1 << 6
};

inline ImageAspectFlags operator|(ImageAspectFlags a, ImageAspectFlags b)
{
    return static_cast<ImageAspectFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(ImageAspectFlags a, ImageAspectFlags b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

inline ImageAspectFlags& operator|=(ImageAspectFlags& a, ImageAspectFlags b)
{
    a = a | b;
    return a;
}

struct ImageSubresourceRange {
    uint32_t BaseMipLevel = 0;
    uint32_t LevelCount = 1;
    uint32_t BaseArrayLayer = 0;
    uint32_t LayerCount = 1;
    ImageAspectFlags AspectMask = ImageAspectFlags::Color;

    static ImageSubresourceRange All()
    {
        return {0, UINT32_MAX, 0, UINT32_MAX};
    }

    static ImageSubresourceRange Mip0()
    {
        return {0, 1, 0, UINT32_MAX};
    }
};

struct CMemoryBarrier {
    ResourceState OldState;
    ResourceState NewState;
};

struct BufferBarrier {
    Ref<Buffer> Buffer;
    ResourceState OldState;
    ResourceState NewState;
    uint64_t Offset = 0;
    uint64_t Size = UINT64_MAX;
    uint32_t SrcQueueFamily = UINT32_MAX;
    uint32_t DstQueueFamily = UINT32_MAX;
};

struct TextureBarrier {
    Ref<Texture> Texture;
    ResourceState OldState;
    ResourceState NewState;
    ImageSubresourceRange SubresourceRange = ImageSubresourceRange::All();
    uint32_t SrcQueueFamily = UINT32_MAX;
    uint32_t DstQueueFamily = UINT32_MAX;
};
} // namespace luna::RHI
#endif
