struct SceneVertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct SceneVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 normal : TEXCOORD1;
};

struct MeshPushConstants
{
    float4x4 model;
    float4x4 viewProjection;
};

#if defined(LUNA_BACKEND_D3D11)
[[vk::push_constant]]
cbuffer PushConstants : register(b13)
{
    MeshPushConstants gPushConstants;
}
#elif defined(LUNA_BACKEND_D3D12)
[[vk::push_constant]]
cbuffer PushConstants : register(b15)
{
    MeshPushConstants gPushConstants;
}
#else
[[vk::push_constant]]
cbuffer PushConstants
{
    MeshPushConstants gPushConstants;
}
#endif

[[vk::binding(0, 0)]]
Texture2D<float4> gAlbedoTexture : register(t0);

[[vk::binding(1, 0)]]
SamplerState gAlbedoSampler : register(s1);

[shader("vertex")]
SceneVertexOutput sceneVertexMain(SceneVertexInput input)
{
    SceneVertexOutput output;
    output.uv = input.uv;
    output.normal = mul((float3x3)gPushConstants.model, input.normal);
    output.position = mul(gPushConstants.viewProjection, mul(gPushConstants.model, float4(input.position, 1.0)));
    return output;
}

[shader("fragment")]
float4 sceneFragmentMain(SceneVertexOutput input) : SV_Target
{
    const float4 albedo = gAlbedoTexture.Sample(gAlbedoSampler, input.uv);
    const float3 normal = normalize(input.normal);
    const float3 lightDirection = normalize(float3(0.45, 0.8, 0.35));
    const float diffuse = max(dot(normal, lightDirection), 0.15);
    return float4(albedo.rgb * diffuse, albedo.a);
}
