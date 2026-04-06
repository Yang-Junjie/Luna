#pragma once

#include "ResourceDescriptors.h"

#include <string_view>
#include <vector>

namespace luna {

struct ShaderDesc {
    ShaderType stage = ShaderType::None;
    std::string_view filePath;
    std::string_view entryPoint = "main";
};

struct VertexAttributeDesc {
    uint32_t location = 0;
    uint32_t offset = 0;
    VertexFormat format = VertexFormat::Undefined;
};

struct VertexBufferLayoutDesc {
    uint32_t stride = 0;
    std::vector<VertexAttributeDesc> attributes;
};

struct ColorAttachmentDesc {
    PixelFormat format = PixelFormat::Undefined;
    bool blendEnabled = false;
};

struct DepthStencilDesc {
    PixelFormat format = PixelFormat::Undefined;
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    CompareOp depthCompareOp = CompareOp::LessOrEqual;
};

struct GraphicsPipelineDesc {
    std::string_view debugName;
    ShaderDesc vertexShader;
    ShaderDesc fragmentShader;
    std::vector<ResourceLayoutHandle> resourceLayouts;
    VertexBufferLayoutDesc vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    PolygonMode polygonMode = PolygonMode::Fill;
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
    uint32_t pushConstantSize = 0;
    ShaderType pushConstantVisibility = ShaderType::AllGraphics;
    std::vector<ColorAttachmentDesc> colorAttachments;
    DepthStencilDesc depthStencil;
    ShaderHandle vertexShaderHandle{};
    ShaderHandle fragmentShaderHandle{};
};

struct ComputePipelineDesc {
    std::string_view debugName;
    ShaderDesc computeShader;
    std::vector<ResourceLayoutHandle> resourceLayouts;
    uint32_t pushConstantSize = 0;
    ShaderType pushConstantVisibility = ShaderType::Compute;
    ShaderHandle computeShaderHandle{};
};

struct SwapchainDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bufferCount = 2;
    PixelFormat format = PixelFormat::BGRA8Unorm;
    bool vsync = true;
};

} // namespace luna

