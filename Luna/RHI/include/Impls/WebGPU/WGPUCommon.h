#ifndef CACAO_WGPU_COMMON_H
#define CACAO_WGPU_COMMON_H

#include <webgpu/webgpu.h>
#include "Core.h"
#include "Barrier.h"
#include "PipelineDefs.h"
#include "Sampler.h"

namespace Cacao
{
    WGPUTextureFormat ToWGPUFormat(Format format);
    Format FromWGPUFormat(WGPUTextureFormat format);
    WGPUBufferUsageFlags ToWGPUBufferUsage(BufferUsageFlags usage);
    WGPUTextureUsageFlags ToWGPUTextureUsage(TextureUsageFlags usage);
    WGPUPrimitiveTopology ToWGPUTopology(PrimitiveTopology topology);
    WGPUVertexFormat ToWGPUVertexFormat(Format format);
    WGPUCullMode ToWGPUCullMode(CullMode mode);
    WGPUFrontFace ToWGPUFrontFace(FrontFace face);
    WGPUFilterMode ToWGPUFilterMode(Filter filter);
    WGPUAddressMode ToWGPUAddressMode(SamplerAddressMode mode);
    WGPUCompareFunction ToWGPUCompareFunction(CompareOp op);
    WGPUBlendFactor ToWGPUBlendFactor(BlendFactor factor);
    WGPUBlendOperation ToWGPUBlendOp(BlendOp op);
    WGPUStencilOperation ToWGPUStencilOp(StencilOp op);
    WGPUColorWriteMaskFlags ToWGPUColorWriteMask(ColorComponentFlags flags);
}

#endif
