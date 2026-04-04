#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace luna {
enum class RHIResult : uint32_t {
    Success = 0,
    NotReady,
    InvalidArgument,
    Unsupported,
    OutOfMemory,
    DeviceLost,
    InternalError
};

enum class RHIBackend : uint32_t {
    Vulkan = 1,
    D3D12 = 2,
    Metal = 3
};

template <typename Tag> struct Handle {
    uint64_t value = 0;

    friend constexpr bool operator==(Handle, Handle) noexcept = default;

    constexpr bool isValid() const noexcept
    {
        return value != 0;
    }

    explicit constexpr operator bool() const noexcept
    {
        return isValid();
    }

    static constexpr Handle fromRaw(uint64_t rawValue) noexcept
    {
        return Handle{rawValue};
    }
};

using DeviceHandle = Handle<struct DeviceHandleTag>;
using BufferHandle = Handle<struct BufferHandleTag>;
using ImageHandle = Handle<struct ImageHandleTag>;
using ImageViewHandle = Handle<struct ImageViewHandleTag>;
using SamplerHandle = Handle<struct SamplerHandleTag>;
using ShaderHandle = Handle<struct ShaderHandleTag>;
using PipelineHandle = Handle<struct PipelineHandleTag>;
using ResourceLayoutHandle = Handle<struct ResourceLayoutHandleTag>;
using ResourceSetHandle = Handle<struct ResourceSetHandleTag>;
using SwapchainHandle = Handle<struct SwapchainHandleTag>;

constexpr std::string_view to_string(RHIResult result) noexcept
{
    switch (result) {
        case RHIResult::Success:
            return "Success";
        case RHIResult::NotReady:
            return "NotReady";
        case RHIResult::InvalidArgument:
            return "InvalidArgument";
        case RHIResult::Unsupported:
            return "Unsupported";
        case RHIResult::OutOfMemory:
            return "OutOfMemory";
        case RHIResult::DeviceLost:
            return "DeviceLost";
        case RHIResult::InternalError:
            return "InternalError";
        default:
            return "Unknown";
    }
}

constexpr std::string_view to_string(RHIBackend backend) noexcept
{
    switch (backend) {
        case RHIBackend::Vulkan:
            return "Vulkan";
        case RHIBackend::D3D12:
            return "D3D12";
        case RHIBackend::Metal:
            return "Metal";
        default:
            return "Unknown";
    }
}

constexpr char ascii_lower(char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

inline bool iequals_ascii(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (size_t index = 0; index < lhs.size(); ++index) {
        if (ascii_lower(lhs[index]) != ascii_lower(rhs[index])) {
            return false;
        }
    }

    return true;
}

inline std::optional<RHIBackend> parse_rhi_backend(std::string_view name) noexcept
{
    if (iequals_ascii(name, "vulkan")) {
        return RHIBackend::Vulkan;
    }

    if (iequals_ascii(name, "d3d12")) {
        return RHIBackend::D3D12;
    }

    if (iequals_ascii(name, "metal")) {
        return RHIBackend::Metal;
    }

    return std::nullopt;
}

enum class ShaderType : uint32_t {
    None = 0,
    Vertex = 1u << 0,
    TessControl = 1u << 1,
    TessEval = 1u << 2,
    Geometry = 1u << 3,
    Fragment = 1u << 4,
    Compute = 1u << 5,

    AllGraphics = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4),
    All = 0xFF'FF'FF'FF
};

inline ShaderType operator|(ShaderType a, ShaderType b)
{
    return static_cast<ShaderType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

enum class ResourceType : uint32_t {
    // 常量缓冲区 (Vulkan: UNIFORM_BUFFER, OpenGL: Uniform Block)
    UniformBuffer,

    // 动态常量缓冲区 (带 Offset，优化频繁更新的数据)
    DynamicUniformBuffer,

    // 采样纹理 (Vulkan: COMBINED_IMAGE_SAMPLER, OpenGL: Sampler2D/Cube)
    CombinedImageSampler,

    // 独立纹理资源 (Vulkan: SAMPLED_IMAGE, 用于 Bindless)
    SampledImage,

    // 独立采样器状态 (Vulkan: SAMPLER)
    Sampler,

    // 存储缓冲区 (Vulkan: STORAGE_BUFFER, 用于 Compute Shader 读写)
    StorageBuffer,

    // 存储图像 (Vulkan: STORAGE_IMAGE, 用于 Compute Shader 写入纹理)
    StorageImage,

    // 输入附件 (Vulkan: INPUT_ATTACHMENT, 用于延迟渲染 Subpass)
    InputAttachment
};

enum class ShaderDataType {
    Float,
    Float2,
    Float3,
    Float4,
    Mat3,
    Mat4,
    Int,
    Int2,
    Int3,
    Int4,
    Bool,
    Struct // 嵌套结构体
};
} // namespace luna
