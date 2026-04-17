#ifndef LUNA_RHI_WGPU_COMMON_H
#define LUNA_RHI_WGPU_COMMON_H

#include "Barrier.h"
#include "Core.h"
#include "PipelineDefs.h"
#include "Sampler.h"

#include <webgpu/webgpu.h>

namespace luna::RHI {
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
} // namespace luna::RHI

#endif
