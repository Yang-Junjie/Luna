#ifndef LUNA_RHI_MTL_COMMON_H
#define LUNA_RHI_MTL_COMMON_H

#ifdef __APPLE__

#include "Barrier.h"
#include "Core.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace luna::RHI {
#ifdef __OBJC__
MTLPixelFormat ToMTLPixelFormat(Format format);
Format FromMTLPixelFormat(MTLPixelFormat format);
MTLPrimitiveType ToMTLPrimitiveType(PrimitiveTopology topology);
MTLVertexFormat ToMTLVertexFormat(Format format);
MTLCullMode ToMTLCullMode(CullMode mode);
MTLWinding ToMTLWinding(FrontFace face);
MTLSamplerMinMagFilter ToMTLFilter(Filter filter);
MTLSamplerAddressMode ToMTLAddressMode(SamplerAddressMode mode);
#endif
} // namespace luna::RHI

#endif // __APPLE__
#endif
