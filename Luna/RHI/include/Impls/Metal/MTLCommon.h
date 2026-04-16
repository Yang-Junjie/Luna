#ifndef CACAO_MTL_COMMON_H
#define CACAO_MTL_COMMON_H

#ifdef __APPLE__

#include "Core.h"
#include "Barrier.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace Cacao
{
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
}

#endif // __APPLE__
#endif
