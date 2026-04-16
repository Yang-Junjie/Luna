#ifndef CACAO_D3D11PIPELINE_H
#define CACAO_D3D11PIPELINE_H
#include "D3D11Common.h"

#include <Pipeline.h>

namespace Cacao {
class D3D11Device;

class CACAO_API D3D11GraphicsPipeline : public GraphicsPipeline {
public:
    D3D11GraphicsPipeline(Ref<D3D11Device> device, const GraphicsPipelineCreateInfo& info);

    ID3D11VertexShader* GetVertexShader() const
    {
        return m_vs.Get();
    }

    ID3D11PixelShader* GetPixelShader() const
    {
        return m_ps.Get();
    }

    ID3D11InputLayout* GetInputLayout() const
    {
        return m_inputLayout.Get();
    }

    ID3D11RasterizerState* GetRasterizerState() const
    {
        return m_rasterState.Get();
    }

    ID3D11DepthStencilState* GetDepthStencilState() const
    {
        return m_depthStencilState.Get();
    }

    ID3D11BlendState* GetBlendState() const
    {
        return m_blendState.Get();
    }

    D3D11_PRIMITIVE_TOPOLOGY GetTopology() const
    {
        return m_topology;
    }

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }

private:
    Ref<D3D11Device> m_device;
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11GeometryShader> m_gs;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11RasterizerState> m_rasterState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    ComPtr<ID3D11BlendState> m_blendState;
    D3D11_PRIMITIVE_TOPOLOGY m_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Ref<PipelineLayout> m_layout;
    UINT m_vertexStride = 0;

public:
    UINT GetVertexStride() const
    {
        return m_vertexStride;
    }
};

class CACAO_API D3D11ComputePipeline : public ComputePipeline {
public:
    D3D11ComputePipeline(Ref<D3D11Device> device, const ComputePipelineCreateInfo& info);

    ID3D11ComputeShader* GetComputeShader() const
    {
        return m_cs.Get();
    }

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }

private:
    Ref<D3D11Device> m_device;
    ComPtr<ID3D11ComputeShader> m_cs;
    Ref<PipelineLayout> m_layout;
};
} // namespace Cacao
#endif
