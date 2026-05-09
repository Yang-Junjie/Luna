static const uint kMaxPointLights = 32;
static const uint kMaxSpotLights = 32;
static const uint kShadowCascadeCount = 4;

struct PostProcessVertexOutput
{
    float4 position : SV_Position;
};

struct SceneParams
{
    float4x4 viewProjection;
    float4x4 inverseViewProjection;
    float4x4 view;
    float4x4 projection;
    float4x4 inverseView;
    float4x4 inverseProjection;
    float4x4 jitteredViewProjection;
    float4x4 inverseJitteredViewProjection;
    float4x4 previousViewProjection;
    float4x4 previousInverseViewProjection;
    float4x4 previousJitteredViewProjection;
    float4x4 previousInverseJitteredViewProjection;
    float4 cameraPositionEnvMip;
    float4 viewportSize;
    float4 previousViewportSize;
    float4 jitterNdc;
    float4 jitterPixels;
    uint4 frameIndices;
    uint4 viewFlags;
    float4 lightDirectionIntensity;
    float4 lightColorExposure;
    float4 iblFactors;
    float4 backgroundColorMode;
    float4 debugOverlayParams;
    float4 debugPickMarker;
    float4x4 shadowViewProjections[kShadowCascadeCount];
    float4 shadowParams;
    float4 shadowCascadeSplits;
    float4 shadowAtlasParams;
    float4 lightCounts;
    float4 pointLightPositionIntensity[kMaxPointLights];
    float4 pointLightColorRange[kMaxPointLights];
    float4 spotLightPositionIntensity[kMaxSpotLights];
    float4 spotLightDirectionRange[kMaxSpotLights];
    float4 spotLightColorCones[kMaxSpotLights];
    float4 spotLightConeParams[kMaxSpotLights];
    float4 irradianceSh[9];
};

[[vk::binding(0, 0)]]
Texture2D<float4> gSceneHdrTexture : register(t0);

[[vk::binding(1, 0)]]
SamplerState gSceneHdrSampler : register(s1);

[[vk::binding(2, 0)]]
ConstantBuffer<SceneParams> gSceneParams : register(b2);

float3 linearToSrgb(float3 color)
{
    return pow(saturate(color), 1.0 / 2.2);
}

float3 toneMapACES(float3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

[shader("vertex")]
PostProcessVertexOutput postProcessVertexMain(uint vertexID : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };

    PostProcessVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 postProcessFragmentMain(PostProcessVertexOutput input) : SV_Target
{
    const float2 uv = saturate(input.position.xy * gSceneParams.viewportSize.zw);
    const float4 sceneColor = gSceneHdrTexture.SampleLevel(gSceneHdrSampler, uv, 0.0);
    const float exposure = max(gSceneParams.lightColorExposure.a, 0.0);
    const float3 hdrColor = min(max(sceneColor.rgb * exposure, float3(0.0)), float3(65504.0));
    return float4(linearToSrgb(toneMapACES(hdrColor)), 1.0);
}
