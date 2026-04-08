#pragma once

#include <cstdint>
#include <type_traits>

namespace luna::render {

struct Extent2D {
    uint32_t width{0};
    uint32_t height{0};
};

struct Extent3D {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t depth{1};
};

enum class PixelFormat : uint8_t {
    Undefined = 0,
    R8G8B8A8Unorm,
    R16G16B16A16Sfloat,
    D32Sfloat,
};

enum class ImageLayout : uint8_t {
    Undefined = 0,
    General,
    ColorAttachment,
    DepthAttachment,
    TransferSrc,
    TransferDst,
    ShaderReadOnly,
    Present,
};

enum class BufferUsage : uint32_t {
    None = 0,
    TransferSrc = 1u << 0,
    TransferDst = 1u << 1,
    Uniform = 1u << 2,
    Index = 1u << 3,
    Storage = 1u << 4,
    ShaderDeviceAddress = 1u << 5,
};

enum class ImageUsage : uint32_t {
    None = 0,
    TransferSrc = 1u << 0,
    TransferDst = 1u << 1,
    Sampled = 1u << 2,
    Storage = 1u << 3,
    ColorAttachment = 1u << 4,
    DepthStencilAttachment = 1u << 5,
};

enum class MemoryUsage : uint8_t {
    GpuOnly = 0,
    CpuOnly,
    CpuToGpu,
};

template <typename T> constexpr T operator|(T lhs, T rhs)
{
    static_assert(std::is_enum_v<T>);
    using Raw = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<Raw>(lhs) | static_cast<Raw>(rhs));
}

template <typename T> constexpr T& operator|=(T& lhs, T rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename T> constexpr bool hasFlag(T value, T flag)
{
    static_assert(std::is_enum_v<T>);
    using Raw = std::underlying_type_t<T>;
    return (static_cast<Raw>(value) & static_cast<Raw>(flag)) != 0;
}

} // namespace luna::render
