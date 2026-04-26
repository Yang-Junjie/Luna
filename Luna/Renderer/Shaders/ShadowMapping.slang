static const uint kMaxPointLights = 32;
static const uint kMaxSpotLights = 32;

struct ShadowVertexInput
{
    float3 position : POSITION;
};

struct ShadowVertexOutput
{
    float4 position : SV_Position;
};

struct ShadowFragmentOutput
{
    float depth : SV_Target0;
};

struct MeshPushConstants
{
    float4x4 model;
    uint pickingId;
    uint3 padding;
};

struct SceneParams
{
    float4x4 viewProjection;
    float4x4 inverseViewProjection;
    float4 cameraPositionEnvMip;
    float4 lightDirectionIntensity;
    float4 lightColorExposure;
    float4 iblFactors;
    float4 debugOverlayParams;
    float4 debugPickMarker;
    float4x4 shadowViewProjection;
    float4 shadowParams;
    float4 lightCounts;
    float4 pointLightPositionIntensity[kMaxPointLights];
    float4 pointLightColorRange[kMaxPointLights];
    float4 spotLightPositionIntensity[kMaxSpotLights];
    float4 spotLightDirectionRange[kMaxSpotLights];
    float4 spotLightColorCones[kMaxSpotLights];
    float4 spotLightConeParams[kMaxSpotLights];
    float4 irradianceSh[9];
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
ConstantBuffer<SceneParams> gSceneParams : register(b0);

[shader("vertex")]
ShadowVertexOutput sceneShadowVertexMain(ShadowVertexInput input)
{
    ShadowVertexOutput output;
    const float4 worldPosition = mul(gPushConstants.model, float4(input.position, 1.0));
    output.position = mul(gSceneParams.shadowViewProjection, worldPosition);
    return output;
}

[shader("fragment")]
ShadowFragmentOutput sceneShadowFragmentMain(ShadowVertexOutput input)
{
    ShadowFragmentOutput output;
    output.depth = input.position.z;
    return output;
}
