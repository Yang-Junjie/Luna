struct SceneLightingVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]]
Texture2D<float4> gGBufferAlbedo : register(t0);

[[vk::binding(1, 0)]]
SamplerState gGBufferSampler : register(s1);

[[vk::binding(2, 0)]]
Texture2D<float4> gGBufferNormal : register(t2);

[shader("vertex")]
SceneLightingVertexOutput sceneLightingVertexMain(uint vertexID : SV_VertexID)
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

    SceneLightingVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.uv = uvs[vertexID];
    return output;
}

[shader("fragment")]
float4 sceneLightingFragmentMain(SceneLightingVertexOutput input) : SV_Target
{
    const float4 albedo = gGBufferAlbedo.Sample(gGBufferSampler, input.uv);
    float3 normal = gGBufferNormal.Sample(gGBufferSampler, input.uv).xyz;

    if (dot(normal, normal) < 1e-4)
    {
        return float4(albedo.rgb, albedo.a);
    }

    normal = normalize(normal);

    const float3 lightDirection = normalize(float3(0.45, 0.8, 0.35));
    const float diffuse = max(dot(normal, lightDirection), 0.15);
    return float4(albedo.rgb * diffuse, albedo.a);
}
