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
    float3 worldNormal : TEXCOORD1;
};

struct GBufferOutput
{
    float4 albedoOpacity : SV_Target0;
    float4 normalReserved : SV_Target1;
};

struct MeshPushConstants
{
    float4x4 model;
    float4x4 viewProjection;
};

struct MaterialParams
{
    float4 alphaParams;
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

[[vk::binding(2, 0)]]
ConstantBuffer<MaterialParams> gMaterialParams : register(b2);

bool isMaskedMaterial()
{
    return gMaterialParams.alphaParams.y > 0.5 && gMaterialParams.alphaParams.y < 1.5;
}

[shader("vertex")]
SceneVertexOutput sceneGeometryVertexMain(SceneVertexInput input)
{
    SceneVertexOutput output;
    const float4 worldPosition = mul(gPushConstants.model, float4(input.position, 1.0));
    output.uv = input.uv;
    output.worldNormal = mul((float3x3)gPushConstants.model, input.normal);
    output.position = mul(gPushConstants.viewProjection, worldPosition);
    return output;
}

[shader("fragment")]
GBufferOutput sceneGeometryFragmentMain(SceneVertexOutput input)
{
    const float4 albedo = gAlbedoTexture.Sample(gAlbedoSampler, input.uv);
    if (isMaskedMaterial() && albedo.a < gMaterialParams.alphaParams.x)
    {
        discard;
    }

    const float3 normal = normalize(input.worldNormal);

    GBufferOutput output;
    output.albedoOpacity = albedo;
    output.normalReserved = float4(normal, 1.0);
    return output;
}

[shader("fragment")]
float4 sceneTransparentFragmentMain(SceneVertexOutput input) : SV_Target
{
    const float4 albedo = gAlbedoTexture.Sample(gAlbedoSampler, input.uv);
    if (albedo.a <= 0.001)
    {
        discard;
    }

    const float3 normal = normalize(input.worldNormal);
    const float3 lightDirection = normalize(float3(0.45, 0.8, 0.35));
    const float diffuse = max(dot(normal, lightDirection), 0.15);
    return float4(albedo.rgb * diffuse, albedo.a);
}
