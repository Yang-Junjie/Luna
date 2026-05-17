struct VisibilityBoundsVertexInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VisibilityBoundsVertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct VisibilityBoundsFragmentOutput
{
    float4 color : SV_Target;
};

struct VisibilityBoundsParams
{
    float4x4 viewProjection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<VisibilityBoundsParams> gVisibilityBoundsParams : register(b0);

[shader("vertex")]
VisibilityBoundsVertexOutput visibilityBoundsOverlayVertexMain(VisibilityBoundsVertexInput input)
{
    VisibilityBoundsVertexOutput output;
    output.position = mul(gVisibilityBoundsParams.viewProjection, float4(input.position, 1.0));
#if defined(LUNA_BACKEND_VULKAN) || defined(LUNA_BACKEND_D3D12)
    output.position.y = -output.position.y;
#endif
    output.color = input.color;
    return output;
}

[shader("fragment")]
VisibilityBoundsFragmentOutput visibilityBoundsOverlayFragmentMain(VisibilityBoundsVertexOutput input)
{
    VisibilityBoundsFragmentOutput output;
    output.color = input.color;
    return output;
}
