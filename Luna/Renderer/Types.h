#pragma once
#include <cstdint>

namespace luna {
enum class ShaderType : uint32_t {
    None = 0,
    Vertex = 1 << 0,      // 000001
    TessControl = 1 << 1, // 000010
    TessEval = 1 << 2,    // 000100
    Geometry = 1 << 3,    // 001000
    Fragment = 1 << 4,    // 010000
    Compute = 1 << 5,     // 100000

    AllGraphics = Vertex | Fragment | Geometry,
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
