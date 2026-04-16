#ifdef __APPLE__

#include "Impls/Metal/MTLCommon.h"
#import <Metal/Metal.h>
#include "PipelineDefs.h"
#include "Sampler.h"

namespace Cacao
{
    MTLPixelFormat ToMTLPixelFormat(Format format)
    {
        switch (format)
        {
        case Format::RGBA8_UNORM:       return MTLPixelFormatRGBA8Unorm;
        case Format::BGRA8_UNORM:       return MTLPixelFormatBGRA8Unorm;
        case Format::RGBA16_FLOAT:      return MTLPixelFormatRGBA16Float;
        case Format::RGBA32_FLOAT:      return MTLPixelFormatRGBA32Float;
        case Format::R8_UNORM:          return MTLPixelFormatR8Unorm;
        case Format::RG8_UNORM:         return MTLPixelFormatRG8Unorm;
        case Format::D32_FLOAT:         return MTLPixelFormatDepth32Float;
        case Format::D24_UNORM_S8_UINT: return MTLPixelFormatDepth24Unorm_Stencil8;
        default:                        return MTLPixelFormatRGBA8Unorm;
        }
    }

    Format FromMTLPixelFormat(MTLPixelFormat format)
    {
        switch (format)
        {
        case MTLPixelFormatRGBA8Unorm:              return Format::RGBA8_UNORM;
        case MTLPixelFormatBGRA8Unorm:              return Format::BGRA8_UNORM;
        case MTLPixelFormatRGBA16Float:             return Format::RGBA16_FLOAT;
        case MTLPixelFormatRGBA32Float:             return Format::RGBA32_FLOAT;
        case MTLPixelFormatR8Unorm:                 return Format::R8_UNORM;
        case MTLPixelFormatRG8Unorm:                return Format::RG8_UNORM;
        case MTLPixelFormatDepth32Float:            return Format::D32_FLOAT;
        case MTLPixelFormatDepth24Unorm_Stencil8:   return Format::D24_UNORM_S8_UINT;
        default:                                    return Format::UNDEFINED;
        }
    }

    MTLPrimitiveType ToMTLPrimitiveType(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::PointList:     return MTLPrimitiveTypePoint;
        case PrimitiveTopology::LineList:      return MTLPrimitiveTypeLine;
        case PrimitiveTopology::LineStrip:     return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::TriangleList:  return MTLPrimitiveTypeTriangle;
        case PrimitiveTopology::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        default:                               return MTLPrimitiveTypeTriangle;
        }
    }

    MTLVertexFormat ToMTLVertexFormat(Format format)
    {
        switch (format)
        {
        case Format::R32_FLOAT:    return MTLVertexFormatFloat;
        case Format::RG32_FLOAT:   return MTLVertexFormatFloat2;
        case Format::RGB32_FLOAT:  return MTLVertexFormatFloat3;
        case Format::RGBA32_FLOAT: return MTLVertexFormatFloat4;
        case Format::R32_UINT:     return MTLVertexFormatUInt;
        case Format::RG32_UINT:    return MTLVertexFormatUInt2;
        case Format::RGB32_UINT:   return MTLVertexFormatUInt3;
        case Format::RGBA32_UINT:  return MTLVertexFormatUInt4;
        default:                   return MTLVertexFormatFloat4;
        }
    }

    MTLCullMode ToMTLCullMode(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None:  return MTLCullModeNone;
        case CullMode::Front: return MTLCullModeFront;
        case CullMode::Back:  return MTLCullModeBack;
        default:              return MTLCullModeNone;
        }
    }

    MTLWinding ToMTLWinding(FrontFace face)
    {
        switch (face)
        {
        case FrontFace::CounterClockwise: return MTLWindingCounterClockwise;
        case FrontFace::Clockwise:        return MTLWindingClockwise;
        default:                          return MTLWindingCounterClockwise;
        }
    }

    MTLSamplerMinMagFilter ToMTLFilter(Filter filter)
    {
        switch (filter)
        {
        case Filter::Nearest: return MTLSamplerMinMagFilterNearest;
        case Filter::Linear:  return MTLSamplerMinMagFilterLinear;
        default:              return MTLSamplerMinMagFilterLinear;
        }
    }

    MTLSamplerAddressMode ToMTLAddressMode(SamplerAddressMode mode)
    {
        switch (mode)
        {
        case SamplerAddressMode::Repeat:         return MTLSamplerAddressModeRepeat;
        case SamplerAddressMode::MirroredRepeat: return MTLSamplerAddressModeMirrorRepeat;
        case SamplerAddressMode::ClampToEdge:    return MTLSamplerAddressModeClampToEdge;
        default:                                 return MTLSamplerAddressModeRepeat;
        }
    }
}

#endif // __APPLE__
