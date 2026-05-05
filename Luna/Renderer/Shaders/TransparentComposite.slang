struct TransparentCompositeVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]]
Texture2D<float4> gTransparentColorTexture : register(t0);

[[vk::binding(1, 0)]]
SamplerState gTransparentColorSampler : register(s1);

[shader("vertex")]
TransparentCompositeVertexOutput transparentCompositeVertexMain(uint vertexID : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };

    const float2 uvs[3] = {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0),
    };

    TransparentCompositeVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.uv = uvs[vertexID];
    return output;
}

[shader("fragment")]
float4 transparentCompositeFragmentMain(TransparentCompositeVertexOutput input) : SV_Target
{
    return gTransparentColorTexture.SampleLevel(gTransparentColorSampler, input.uv, 0.0);
}
