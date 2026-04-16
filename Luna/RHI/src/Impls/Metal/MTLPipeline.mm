#ifdef __APPLE__
#include "Impls/Metal/MTLPipeline.h"
#include "Impls/Metal/MTLDevice.h"
#import <Metal/Metal.h>

namespace Cacao
{
    static MTLVertexFormat ToMTLVertexFormat(Format format)
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
        case Format::R32_SINT:     return MTLVertexFormatInt;
        case Format::RG32_SINT:    return MTLVertexFormatInt2;
        case Format::RGB32_SINT:   return MTLVertexFormatInt3;
        case Format::RGBA32_SINT:  return MTLVertexFormatInt4;
        case Format::R16_FLOAT:    return MTLVertexFormatHalf;
        case Format::RG16_FLOAT:   return MTLVertexFormatHalf2;
        case Format::RGBA16_FLOAT: return MTLVertexFormatHalf4;
        case Format::RGBA8_UNORM:  return MTLVertexFormatUChar4Normalized;
        case Format::RGBA8_SNORM:  return MTLVertexFormatChar4Normalized;
        default:                   return MTLVertexFormatFloat4;
        }
    }

    static MTLPixelFormat ToMTLPixelFormat(Format format)
    {
        switch (format)
        {
        case Format::R8_UNORM:       return MTLPixelFormatR8Unorm;
        case Format::RG8_UNORM:      return MTLPixelFormatRG8Unorm;
        case Format::RGBA8_UNORM:    return MTLPixelFormatRGBA8Unorm;
        case Format::RGBA8_SRGB:     return MTLPixelFormatRGBA8Unorm_sRGB;
        case Format::BGRA8_UNORM:    return MTLPixelFormatBGRA8Unorm;
        case Format::BGRA8_SRGB:     return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::R16_FLOAT:      return MTLPixelFormatR16Float;
        case Format::RG16_FLOAT:     return MTLPixelFormatRG16Float;
        case Format::RGBA16_FLOAT:   return MTLPixelFormatRGBA16Float;
        case Format::R32_FLOAT:      return MTLPixelFormatR32Float;
        case Format::RG32_FLOAT:     return MTLPixelFormatRG32Float;
        case Format::RGBA32_FLOAT:   return MTLPixelFormatRGBA32Float;
        case Format::D32_FLOAT:      return MTLPixelFormatDepth32Float;
        case Format::D24_UNORM_S8_UINT: return MTLPixelFormatDepth24Unorm_Stencil8;
        case Format::D32_FLOAT_S8_UINT: return MTLPixelFormatDepth32Float_Stencil8;
        case Format::S8_UINT:        return MTLPixelFormatStencil8;
        case Format::RGB10A2_UNORM:  return MTLPixelFormatRGB10A2Unorm;
        case Format::RG11B10_FLOAT:  return MTLPixelFormatRG11B10Float;
        default:                     return MTLPixelFormatInvalid;
        }
    }

    static MTLBlendFactor ToMTLBlendFactor(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero:              return MTLBlendFactorZero;
        case BlendFactor::One:               return MTLBlendFactorOne;
        case BlendFactor::SrcColor:          return MTLBlendFactorSourceColor;
        case BlendFactor::OneMinusSrcColor:  return MTLBlendFactorOneMinusSourceColor;
        case BlendFactor::DstColor:          return MTLBlendFactorDestinationColor;
        case BlendFactor::OneMinusDstColor:  return MTLBlendFactorOneMinusDestinationColor;
        case BlendFactor::SrcAlpha:          return MTLBlendFactorSourceAlpha;
        case BlendFactor::OneMinusSrcAlpha:  return MTLBlendFactorOneMinusSourceAlpha;
        case BlendFactor::DstAlpha:          return MTLBlendFactorDestinationAlpha;
        case BlendFactor::OneMinusDstAlpha:  return MTLBlendFactorOneMinusDestinationAlpha;
        case BlendFactor::SrcAlphaSaturate:  return MTLBlendFactorSourceAlphaSaturated;
        default:                             return MTLBlendFactorOne;
        }
    }

    static MTLBlendOperation ToMTLBlendOp(BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Add:             return MTLBlendOperationAdd;
        case BlendOp::Subtract:        return MTLBlendOperationSubtract;
        case BlendOp::ReverseSubtract: return MTLBlendOperationReverseSubtract;
        case BlendOp::Min:             return MTLBlendOperationMin;
        case BlendOp::Max:             return MTLBlendOperationMax;
        default:                       return MTLBlendOperationAdd;
        }
    }

    static MTLCompareFunction ToMTLCompareFunction(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Never:          return MTLCompareFunctionNever;
        case CompareOp::Less:           return MTLCompareFunctionLess;
        case CompareOp::Equal:          return MTLCompareFunctionEqual;
        case CompareOp::LessOrEqual:    return MTLCompareFunctionLessEqual;
        case CompareOp::Greater:        return MTLCompareFunctionGreater;
        case CompareOp::NotEqual:       return MTLCompareFunctionNotEqual;
        case CompareOp::GreaterOrEqual: return MTLCompareFunctionGreaterEqual;
        case CompareOp::Always:         return MTLCompareFunctionAlways;
        default:                        return MTLCompareFunctionAlways;
        }
    }

    MTLGraphicsPipeline::MTLGraphicsPipeline(const Ref<Device>& device, const GraphicsPipelineCreateInfo& info)
    {
        m_layout = info.Layout;
        auto mtlDevice = std::dynamic_pointer_cast<MTLDevice>(device);
        if (!mtlDevice) return;

        id<MTLDevice> dev = (id<MTLDevice>)mtlDevice->GetHandle();
        if (!dev) return;

        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];

        // Vertex descriptor
        MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
        for (auto& binding : info.VertexBindings)
        {
            vertexDesc.layouts[binding.Binding].stride = binding.Stride;
            vertexDesc.layouts[binding.Binding].stepRate = 1;
            vertexDesc.layouts[binding.Binding].stepFunction =
                (binding.InputRate == VertexInputRate::Instance)
                ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
        }
        for (auto& attr : info.VertexAttributes)
        {
            vertexDesc.attributes[attr.Location].format = ToMTLVertexFormat(attr.Format);
            vertexDesc.attributes[attr.Location].offset = attr.Offset;
            vertexDesc.attributes[attr.Location].bufferIndex = attr.Binding;
        }
        desc.vertexDescriptor = vertexDesc;

        // Shader functions from compiled Metal library (metallib bytecode in ShaderBlob)
        for (auto& shader : info.Shaders)
        {
            if (!shader) continue;
            auto& blob = shader->GetBlob();
            NSError* error = nil;
            dispatch_data_t data = dispatch_data_create(
                blob.Data.data(), blob.Data.size(),
                dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            id<MTLLibrary> lib = [dev newLibraryWithData:data error:&error];
            if (!lib) continue;

            id<MTLFunction> func = [lib newFunctionWithName:@"main0"];
            if (!func) func = [lib newFunctionWithName:
                [NSString stringWithUTF8String:shader->GetEntryPoint().c_str()]];
            if (!func) continue;

            if (shader->GetStage() == ShaderStage::Vertex)
                desc.vertexFunction = func;
            else if (shader->GetStage() == ShaderStage::Fragment)
                desc.fragmentFunction = func;
        }

        // Color attachments
        for (size_t i = 0; i < info.ColorAttachmentFormats.size(); i++)
        {
            desc.colorAttachments[i].pixelFormat = ToMTLPixelFormat(info.ColorAttachmentFormats[i]);
            if (i < info.ColorBlend.Attachments.size())
            {
                auto& att = info.ColorBlend.Attachments[i];
                desc.colorAttachments[i].blendingEnabled = att.BlendEnable;
                desc.colorAttachments[i].sourceRGBBlendFactor = ToMTLBlendFactor(att.SrcColorBlendFactor);
                desc.colorAttachments[i].destinationRGBBlendFactor = ToMTLBlendFactor(att.DstColorBlendFactor);
                desc.colorAttachments[i].rgbBlendOperation = ToMTLBlendOp(att.ColorBlendOp);
                desc.colorAttachments[i].sourceAlphaBlendFactor = ToMTLBlendFactor(att.SrcAlphaBlendFactor);
                desc.colorAttachments[i].destinationAlphaBlendFactor = ToMTLBlendFactor(att.DstAlphaBlendFactor);
                desc.colorAttachments[i].alphaBlendOperation = ToMTLBlendOp(att.AlphaBlendOp);
                MTLColorWriteMask mask = MTLColorWriteMaskNone;
                if (att.ColorWriteMask & ColorComponentFlags::R) mask |= MTLColorWriteMaskRed;
                if (att.ColorWriteMask & ColorComponentFlags::G) mask |= MTLColorWriteMaskGreen;
                if (att.ColorWriteMask & ColorComponentFlags::B) mask |= MTLColorWriteMaskBlue;
                if (att.ColorWriteMask & ColorComponentFlags::A) mask |= MTLColorWriteMaskAlpha;
                desc.colorAttachments[i].writeMask = mask;
            }
        }

        // Depth/stencil
        if (info.DepthStencilFormat != Format::UNDEFINED)
            desc.depthAttachmentPixelFormat = ToMTLPixelFormat(info.DepthStencilFormat);

        // Rasterization
        desc.rasterSampleCount = std::max(1u, info.Multisample.RasterizationSamples);
        desc.alphaToCoverageEnabled = info.Multisample.AlphaToCoverageEnable;
        desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;

        NSError* error = nil;
        m_pipelineState = [dev newRenderPipelineStateWithDescriptor:desc error:&error];

        // Depth stencil state is separate in Metal
        if (info.DepthStencilFormat != Format::UNDEFINED)
        {
            MTLDepthStencilDescriptor* dsDesc = [[MTLDepthStencilDescriptor alloc] init];
            dsDesc.depthWriteEnabled = info.DepthStencil.DepthWriteEnable;
            dsDesc.depthCompareFunction = info.DepthStencil.DepthTestEnable
                ? ToMTLCompareFunction(info.DepthStencil.DepthCompareOp)
                : MTLCompareFunctionAlways;
            // Depth stencil state stored separately; commands bind it at draw time
            // id<MTLDepthStencilState> dsState = [dev newDepthStencilStateWithDescriptor:dsDesc];
        }
    }

    MTLComputePipeline::MTLComputePipeline(const Ref<Device>& device, const ComputePipelineCreateInfo& info)
    {
        m_layout = info.Layout;
        auto mtlDevice = std::dynamic_pointer_cast<MTLDevice>(device);
        if (!mtlDevice || !info.ComputeShader) return;

        id<MTLDevice> dev = (id<MTLDevice>)mtlDevice->GetHandle();
        if (!dev) return;

        auto& blob = info.ComputeShader->GetBlob();
        NSError* error = nil;
        dispatch_data_t data = dispatch_data_create(
            blob.Data.data(), blob.Data.size(),
            dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        id<MTLLibrary> lib = [dev newLibraryWithData:data error:&error];
        if (!lib) return;

        id<MTLFunction> func = [lib newFunctionWithName:@"main0"];
        if (!func) func = [lib newFunctionWithName:
            [NSString stringWithUTF8String:info.ComputeShader->GetEntryPoint().c_str()]];
        if (!func) return;

        m_pipelineState = [dev newComputePipelineStateWithFunction:func error:&error];
    }
}
#endif // __APPLE__
