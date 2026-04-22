#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Pipeline.h"
#include "Impls/D3D11/D3D11Sampler.h"
#include "Impls/D3D11/D3D11ShaderModule.h"

namespace luna::RHI {
static D3D11_CULL_MODE ToD3D11CullMode(CullMode mode)
{
    switch (mode) {
        case CullMode::None:
            return D3D11_CULL_NONE;
        case CullMode::Front:
            return D3D11_CULL_FRONT;
        case CullMode::Back:
            return D3D11_CULL_BACK;
        case CullMode::FrontAndBack:
            return D3D11_CULL_NONE;
        default:
            return D3D11_CULL_BACK;
    }
}

static D3D11_FILL_MODE ToD3D11FillMode(PolygonMode mode)
{
    switch (mode) {
        case PolygonMode::Fill:
            return D3D11_FILL_SOLID;
        case PolygonMode::Line:
            return D3D11_FILL_WIREFRAME;
        default:
            return D3D11_FILL_SOLID;
    }
}

static D3D11_BLEND ToD3D11Blend(BlendFactor factor)
{
    switch (factor) {
        case BlendFactor::Zero:
            return D3D11_BLEND_ZERO;
        case BlendFactor::One:
            return D3D11_BLEND_ONE;
        case BlendFactor::SrcColor:
            return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:
            return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::DstColor:
            return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::OneMinusDstColor:
            return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlpha:
            return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:
            return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:
            return D3D11_BLEND_INV_DEST_ALPHA;
        case BlendFactor::ConstantColor:
            return D3D11_BLEND_BLEND_FACTOR;
        case BlendFactor::OneMinusConstantColor:
            return D3D11_BLEND_INV_BLEND_FACTOR;
        case BlendFactor::SrcAlphaSaturate:
            return D3D11_BLEND_SRC_ALPHA_SAT;
        default:
            return D3D11_BLEND_ONE;
    }
}

static D3D11_BLEND_OP ToD3D11BlendOp(BlendOp op)
{
    switch (op) {
        case BlendOp::Add:
            return D3D11_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return D3D11_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract:
            return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min:
            return D3D11_BLEND_OP_MIN;
        case BlendOp::Max:
            return D3D11_BLEND_OP_MAX;
        default:
            return D3D11_BLEND_OP_ADD;
    }
}

static D3D11_STENCIL_OP ToD3D11StencilOp(StencilOp op)
{
    switch (op) {
        case StencilOp::Keep:
            return D3D11_STENCIL_OP_KEEP;
        case StencilOp::Zero:
            return D3D11_STENCIL_OP_ZERO;
        case StencilOp::Replace:
            return D3D11_STENCIL_OP_REPLACE;
        case StencilOp::IncrementAndClamp:
            return D3D11_STENCIL_OP_INCR_SAT;
        case StencilOp::DecrementAndClamp:
            return D3D11_STENCIL_OP_DECR_SAT;
        case StencilOp::Invert:
            return D3D11_STENCIL_OP_INVERT;
        case StencilOp::IncrementWrap:
            return D3D11_STENCIL_OP_INCR;
        case StencilOp::DecrementWrap:
            return D3D11_STENCIL_OP_DECR;
        default:
            return D3D11_STENCIL_OP_KEEP;
    }
}

D3D11GraphicsPipeline::D3D11GraphicsPipeline(Ref<D3D11Device> device, const GraphicsPipelineCreateInfo& info)
    : m_device(std::move(device)),
      m_layout(info.Layout)
{
    auto* dev = m_device->GetNativeDevice();
    D3D11ShaderModule* vsModule = nullptr;
    D3D11ShaderModule* psModule = nullptr;
    D3D11ShaderModule* gsModule = nullptr;

    for (auto& shader : info.Shaders) {
        auto* mod = static_cast<D3D11ShaderModule*>(shader.get());
        if (mod->GetStage() == ShaderStage::Vertex) {
            vsModule = mod;
        } else if (mod->GetStage() == ShaderStage::Fragment) {
            psModule = mod;
        } else if (mod->GetStage() == ShaderStage::Geometry) {
            gsModule = mod;
        }
    }

    if (vsModule) {
        auto bc = vsModule->GetBytecode();
        dev->CreateVertexShader(bc.data(), bc.size(), nullptr, &m_vs);

        std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
        for (auto& attr : info.VertexAttributes) {
            D3D11_INPUT_ELEMENT_DESC elem{};
            elem.SemanticName = attr.SemanticName.c_str();
            elem.SemanticIndex = (attr.SemanticIndex != UINT32_MAX) ? attr.SemanticIndex : 0;
            elem.Format = D3D11_ToDXGIFormat(attr.Format);
            elem.InputSlot = attr.Binding;
            elem.AlignedByteOffset = attr.Offset;
            for (auto& binding : info.VertexBindings) {
                if (binding.Binding == attr.Binding) {
                    elem.InputSlotClass = (binding.InputRate == VertexInputRate::Instance)
                                              ? D3D11_INPUT_PER_INSTANCE_DATA
                                              : D3D11_INPUT_PER_VERTEX_DATA;
                    elem.InstanceDataStepRate = (binding.InputRate == VertexInputRate::Instance) ? 1 : 0;
                    break;
                }
            }
            elements.push_back(elem);
        }
        if (!elements.empty()) {
            dev->CreateInputLayout(
                elements.data(), static_cast<UINT>(elements.size()), bc.data(), bc.size(), &m_inputLayout);
        }
    }

    if (!info.VertexBindings.empty()) {
        m_vertexStride = info.VertexBindings[0].Stride;
    }

    if (psModule) {
        auto bc = psModule->GetBytecode();
        dev->CreatePixelShader(bc.data(), bc.size(), nullptr, &m_ps);
    }

    if (gsModule) {
        auto bc = gsModule->GetBytecode();
        dev->CreateGeometryShader(bc.data(), bc.size(), nullptr, &m_gs);
    }

    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = ToD3D11FillMode(info.Rasterizer.PolygonMode);
        rd.CullMode = ToD3D11CullMode(info.Rasterizer.CullMode);
        rd.FrontCounterClockwise = (info.Rasterizer.FrontFace == FrontFace::CounterClockwise);
        rd.DepthBias = static_cast<INT>(info.Rasterizer.DepthBiasConstantFactor);
        rd.DepthBiasClamp = info.Rasterizer.DepthBiasClamp;
        rd.SlopeScaledDepthBias = info.Rasterizer.DepthBiasSlopeFactor;
        rd.DepthClipEnable = !info.Rasterizer.DepthClampEnable;
        rd.ScissorEnable = TRUE;
        rd.MultisampleEnable = (info.Multisample.RasterizationSamples > 1);
        rd.AntialiasedLineEnable = FALSE;
        dev->CreateRasterizerState(&rd, &m_rasterState);
    }

    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable = info.DepthStencil.DepthTestEnable;
        dsd.DepthWriteMask =
            info.DepthStencil.DepthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = D3D11_ToCompareFunc(info.DepthStencil.DepthCompareOp);
        dsd.StencilEnable = info.DepthStencil.StencilTestEnable;
        dsd.StencilReadMask = static_cast<UINT8>(info.DepthStencil.StencilReadMask);
        dsd.StencilWriteMask = static_cast<UINT8>(info.DepthStencil.StencilWriteMask);

        auto mapStencilFace = [](const StencilOpState& s) -> D3D11_DEPTH_STENCILOP_DESC {
            return {ToD3D11StencilOp(s.FailOp),
                    ToD3D11StencilOp(s.DepthFailOp),
                    ToD3D11StencilOp(s.PassOp),
                    D3D11_ToCompareFunc(s.CompareOp)};
        };
        dsd.FrontFace = mapStencilFace(info.DepthStencil.Front);
        dsd.BackFace = mapStencilFace(info.DepthStencil.Back);
        dev->CreateDepthStencilState(&dsd, &m_depthStencilState);
    }

    {
        D3D11_BLEND_DESC bd{};
        bd.AlphaToCoverageEnable = info.Multisample.AlphaToCoverageEnable;
        bd.IndependentBlendEnable = (info.ColorBlend.Attachments.size() > 1);
        for (size_t i = 0; i < info.ColorBlend.Attachments.size() && i < 8; ++i) {
            auto& src = info.ColorBlend.Attachments[i];
            auto& dst = bd.RenderTarget[i];
            dst.BlendEnable = src.BlendEnable;
            dst.SrcBlend = ToD3D11Blend(src.SrcColorBlendFactor);
            dst.DestBlend = ToD3D11Blend(src.DstColorBlendFactor);
            dst.BlendOp = ToD3D11BlendOp(src.ColorBlendOp);
            dst.SrcBlendAlpha = ToD3D11Blend(src.SrcAlphaBlendFactor);
            dst.DestBlendAlpha = ToD3D11Blend(src.DstAlphaBlendFactor);
            dst.BlendOpAlpha = ToD3D11BlendOp(src.AlphaBlendOp);
            dst.RenderTargetWriteMask = 0;
            if (src.ColorWriteMask & ColorComponentFlags::R) {
                dst.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_RED;
            }
            if (src.ColorWriteMask & ColorComponentFlags::G) {
                dst.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
            }
            if (src.ColorWriteMask & ColorComponentFlags::B) {
                dst.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
            }
            if (src.ColorWriteMask & ColorComponentFlags::A) {
                dst.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
            }
        }
        if (info.ColorBlend.Attachments.empty()) {
            bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        dev->CreateBlendState(&bd, &m_blendState);
    }

    switch (info.InputAssembly.Topology) {
        case PrimitiveTopology::PointList:
            m_topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case PrimitiveTopology::LineList:
            m_topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case PrimitiveTopology::LineStrip:
            m_topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case PrimitiveTopology::TriangleList:
            m_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case PrimitiveTopology::TriangleStrip:
            m_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case PrimitiveTopology::PatchList:
            m_topology = D3D11_PRIMITIVE_TOPOLOGY(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST +
                                                  info.InputAssembly.PatchControlPoints - 1);
            break;
        default:
            m_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
    }
}

D3D11ComputePipeline::D3D11ComputePipeline(Ref<D3D11Device> device, const ComputePipelineCreateInfo& info)
    : m_device(std::move(device)),
      m_layout(info.Layout)
{
    if (info.ComputeShader) {
        auto* mod = static_cast<D3D11ShaderModule*>(info.ComputeShader.get());
        auto bc = mod->GetBytecode();
        m_device->GetNativeDevice()->CreateComputeShader(bc.data(), bc.size(), nullptr, &m_cs);
    }
}
} // namespace luna::RHI
