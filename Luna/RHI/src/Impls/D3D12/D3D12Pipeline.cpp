#include "Impls/D3D12/D3D12Common.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12Pipeline.h"
#include "Impls/D3D12/D3D12PipelineLayout.h"

#include <cstdio>
#include <cstdlib>

#include <atomic>
#include <sstream>
#include <stdexcept>

namespace {
std::atomic<uint64_t> g_psoCounter{0};

D3D12_BLEND ToD3D12Blend(luna::RHI::BlendFactor factor)
{
    using namespace luna::RHI;

    switch (factor) {
        case BlendFactor::Zero:
            return D3D12_BLEND_ZERO;
        case BlendFactor::One:
            return D3D12_BLEND_ONE;
        case BlendFactor::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::DstColor:
            return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::OneMinusDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::ConstantColor:
            return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::OneMinusConstantColor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case BlendFactor::SrcAlphaSaturate:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        default:
            return D3D12_BLEND_ONE;
    }
}

D3D12_BLEND_OP ToD3D12BlendOp(luna::RHI::BlendOp op)
{
    using namespace luna::RHI;

    switch (op) {
        case BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
    }
}

UINT8 ToD3D12ColorWriteMask(luna::RHI::ColorComponentFlags mask)
{
    using namespace luna::RHI;

    UINT8 result = 0;
    if (mask & ColorComponentFlags::R) {
        result |= D3D12_COLOR_WRITE_ENABLE_RED;
    }
    if (mask & ColorComponentFlags::G) {
        result |= D3D12_COLOR_WRITE_ENABLE_GREEN;
    }
    if (mask & ColorComponentFlags::B) {
        result |= D3D12_COLOR_WRITE_ENABLE_BLUE;
    }
    if (mask & ColorComponentFlags::A) {
        result |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
    }
    return result;
}
} // namespace

namespace luna::RHI {
D3D12GraphicsPipeline::D3D12GraphicsPipeline(const Ref<Device>& device, const GraphicsPipelineCreateInfo& info)
    : m_device(device),
      m_layout(info.Layout),
      m_vertexBindings(info.VertexBindings)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    if (info.Layout) {
        auto d3dLayout = std::dynamic_pointer_cast<D3D12PipelineLayout>(info.Layout);
        if (d3dLayout && d3dLayout->GetHandle()) {
            m_rootSignature = d3dLayout->GetHandle();
        }
    }

    if (!m_rootSignature) {
        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> signatureBlob, errorBlob;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "D3D12SerializeRootSignature failed: 0x%08X", static_cast<unsigned>(hr));
            throw std::runtime_error(buf);
        }
        hr = d3dDevice->GetHandle()->CreateRootSignature(
            0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        if (FAILED(hr)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "ID3D12Device::CreateRootSignature failed: 0x%08X", static_cast<unsigned>(hr));
            throw std::runtime_error(buf);
        }
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();

    // Shader bytecode from ShaderModule(s)
    for (auto& shader : info.Shaders) {
        auto& blob = shader->GetBlob();
        if (shader->GetStage() & ShaderStage::Vertex) {
            psoDesc.VS = {blob.Data.data(), blob.Data.size()};
        } else if (shader->GetStage() & ShaderStage::Fragment) {
            psoDesc.PS = {blob.Data.data(), blob.Data.size()};
        }
    }

    // Input layout from VertexAttributes
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
    for (auto& attr : info.VertexAttributes) {
        D3D12_INPUT_ELEMENT_DESC elem = {};
        elem.SemanticName = attr.SemanticName.c_str();
        elem.SemanticIndex = (attr.SemanticIndex != UINT32_MAX) ? attr.SemanticIndex : 0;
        elem.Format = ToDXGIFormat(attr.Format);
        elem.InputSlot = attr.Binding;
        elem.AlignedByteOffset = attr.Offset;
        elem.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        inputElements.push_back(elem);
    }
    psoDesc.InputLayout = {inputElements.data(), static_cast<UINT>(inputElements.size())};

    // Rasterizer
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    switch (info.Rasterizer.CullMode) {
        case CullMode::None:
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            break;
        case CullMode::Front:
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
            break;
        case CullMode::Back:
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
            break;
        default:
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
            break;
    }
    psoDesc.RasterizerState.FrontCounterClockwise =
        (info.Rasterizer.FrontFace == FrontFace::CounterClockwise) ? TRUE : FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // Depth-Stencil
    psoDesc.DepthStencilState.DepthEnable = info.DepthStencil.DepthTestEnable ? TRUE : FALSE;
    psoDesc.DepthStencilState.DepthWriteMask =
        info.DepthStencil.DepthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    // Blend
    psoDesc.BlendState.AlphaToCoverageEnable = info.Multisample.AlphaToCoverageEnable ? TRUE : FALSE;
    psoDesc.BlendState.IndependentBlendEnable = info.ColorBlend.Attachments.size() > 1 ? TRUE : FALSE;
    for (size_t i = 0; i < info.ColorBlend.Attachments.size() && i < 8; ++i) {
        const auto& src = info.ColorBlend.Attachments[i];
        auto& dst = psoDesc.BlendState.RenderTarget[i];
        dst.BlendEnable = src.BlendEnable ? TRUE : FALSE;
        dst.LogicOpEnable = FALSE;
        dst.SrcBlend = ToD3D12Blend(src.SrcColorBlendFactor);
        dst.DestBlend = ToD3D12Blend(src.DstColorBlendFactor);
        dst.BlendOp = ToD3D12BlendOp(src.ColorBlendOp);
        dst.SrcBlendAlpha = ToD3D12Blend(src.SrcAlphaBlendFactor);
        dst.DestBlendAlpha = ToD3D12Blend(src.DstAlphaBlendFactor);
        dst.BlendOpAlpha = ToD3D12BlendOp(src.AlphaBlendOp);
        dst.LogicOp = D3D12_LOGIC_OP_NOOP;
        dst.RenderTargetWriteMask = ToD3D12ColorWriteMask(src.ColorWriteMask);
    }
    if (info.ColorBlend.Attachments.empty()) {
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // Multisample
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = info.Multisample.RasterizationSamples;

    // Topology
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // Render target formats
    psoDesc.NumRenderTargets = static_cast<UINT>(info.ColorAttachmentFormats.size());
    for (uint32_t i = 0; i < info.ColorAttachmentFormats.size() && i < 8; i++) {
        psoDesc.RTVFormats[i] = ToDXGIFormat(info.ColorAttachmentFormats[i]);
    }

    if (info.DepthStencilFormat != Format::UNDEFINED) {
        psoDesc.DSVFormat = ToDXGIFormat(info.DepthStencilFormat);
    }

    if (psoDesc.SampleDesc.Count == 0) {
        psoDesc.SampleDesc.Count = 1;
    }

    wchar_t psoName[64];
    swprintf_s(psoName, L"gfx_%llu", g_psoCounter.fetch_add(1));

    auto* lib = d3dDevice->GetPipelineLibrary();
    HRESULT hr = E_FAIL;
    if (lib) {
        hr = lib->LoadGraphicsPipeline(psoName, &psoDesc, IID_PPV_ARGS(&m_pipelineState));
    }
    if (FAILED(hr)) {
        hr = d3dDevice->GetHandle()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
        if (FAILED(hr)) {
            std::ostringstream error;
            error << "ID3D12Device::CreateGraphicsPipelineState failed: 0x";
            char hrBuffer[16];
            snprintf(hrBuffer, sizeof(hrBuffer), "%08X", static_cast<unsigned>(hr));
            error << hrBuffer;

            ComPtr<ID3D12InfoQueue> infoQueue;
            if (SUCCEEDED(d3dDevice->GetHandle()->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
                const UINT64 messageCount = infoQueue->GetNumStoredMessages();
                bool appendedMessage = false;
                for (UINT64 i = 0; i < messageCount; ++i) {
                    SIZE_T messageSize = 0;
                    infoQueue->GetMessage(i, nullptr, &messageSize);
                    auto* message = static_cast<D3D12_MESSAGE*>(malloc(messageSize));
                    if (message == nullptr) {
                        continue;
                    }

                    if (SUCCEEDED(infoQueue->GetMessage(i, message, &messageSize)) &&
                        message->pDescription != nullptr) {
                        error << (appendedMessage ? " | " : " [");
                        error << message->pDescription;
                        appendedMessage = true;
                    }
                    free(message);
                }

                if (appendedMessage) {
                    error << "]";
                }
                infoQueue->ClearStoredMessages();
            }

            throw std::runtime_error(error.str());
        } else {
            d3dDevice->StorePSO(psoName, m_pipelineState.Get());
        }
    }
    m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

D3D12ComputePipeline::D3D12ComputePipeline(const Ref<Device>& device, const ComputePipelineCreateInfo& info)
    : m_device(device),
      m_layout(info.Layout)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    if (auto d3dLayout = std::dynamic_pointer_cast<D3D12PipelineLayout>(info.Layout)) {
        m_rootSignature = ComPtr<ID3D12RootSignature>(d3dLayout->GetHandle());
    }

    if (!m_rootSignature) {
        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        ComPtr<ID3DBlob> signatureBlob, errorBlob;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "D3D12SerializeRootSignature failed: 0x%08X", static_cast<unsigned>(hr));
            throw std::runtime_error(buf);
        }
        hr = d3dDevice->GetHandle()->CreateRootSignature(
            0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        if (FAILED(hr)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "ID3D12Device::CreateRootSignature failed: 0x%08X", static_cast<unsigned>(hr));
            throw std::runtime_error(buf);
        }
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();

    if (info.ComputeShader) {
        auto& blob = info.ComputeShader->GetBlob();
        psoDesc.CS = {blob.Data.data(), blob.Data.size()};
    }

    HRESULT hr = d3dDevice->GetHandle()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    if (FAILED(hr)) {
        std::ostringstream error;
        error << "ID3D12Device::CreateComputePipelineState failed: 0x";
        char hrBuffer[16];
        snprintf(hrBuffer, sizeof(hrBuffer), "%08X", static_cast<unsigned>(hr));
        error << hrBuffer;

        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(d3dDevice->GetHandle()->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
            const UINT64 messageCount = infoQueue->GetNumStoredMessages();
            bool appendedMessage = false;
            for (UINT64 i = 0; i < messageCount; ++i) {
                SIZE_T messageSize = 0;
                infoQueue->GetMessage(i, nullptr, &messageSize);
                auto* message = static_cast<D3D12_MESSAGE*>(malloc(messageSize));
                if (message == nullptr) {
                    continue;
                }

                if (SUCCEEDED(infoQueue->GetMessage(i, message, &messageSize)) && message->pDescription != nullptr) {
                    error << (appendedMessage ? " | " : " [");
                    error << message->pDescription;
                    appendedMessage = true;
                }
                free(message);
            }

            if (appendedMessage) {
                error << "]";
            }
            infoQueue->ClearStoredMessages();
        }

        throw std::runtime_error(error.str());
    }
}
} // namespace luna::RHI
