#pragma once

#include "Descriptors.h"

#include <cstdint>

namespace luna {

struct ClearColorValue {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct DrawArguments {
    uint32_t vertexCount = 0;
    uint32_t instanceCount = 1;
    uint32_t firstVertex = 0;
    uint32_t firstInstance = 0;
};

class IRHICommandContext {
public:
    virtual ~IRHICommandContext() = default;

    virtual RHIResult clearColor(const ClearColorValue& color) = 0;
    virtual RHIResult bindGraphicsPipeline(PipelineHandle pipeline) = 0;
    virtual RHIResult bindVertexBuffer(BufferHandle buffer, uint64_t offset = 0) = 0;
    virtual RHIResult draw(const DrawArguments& arguments) = 0;
};

} // namespace luna
