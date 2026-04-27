struct SsaoVertexOutput
{
    float4 position : SV_Position;
};

struct SsaoParams
{
    float4 framebuffer;
    float4 settings;
};

[[vk::binding(0, 0)]]
Texture2D<float> gDepthTexture : register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> gNormalMetallicTexture : register(t1);

[[vk::binding(2, 0)]]
Texture2D<float4> gWorldPositionRoughnessTexture : register(t2);

[[vk::binding(3, 0)]]
SamplerState gSsaoSampler : register(s3);

[[vk::binding(4, 0)]]
ConstantBuffer<SsaoParams> gSsaoParams : register(b4);

float3 safeNormalize(float3 value, float3 fallbackValue)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 1e-6 ? value * rsqrt(lengthSq) : fallbackValue;
}

float2 screenUvFromPosition(float4 position)
{
    return saturate(position.xy * gSsaoParams.framebuffer.xy);
}

bool isInsideScreen(float2 uv)
{
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

bool loadSurface(float2 uv, out float depth, out float3 normal, out float3 position)
{
    depth = gDepthTexture.SampleLevel(gSsaoSampler, uv, 0.0);
    const float4 normalMetallic = gNormalMetallicTexture.SampleLevel(gSsaoSampler, uv, 0.0);
    normal = safeNormalize(normalMetallic.xyz, float3(0.0, 0.0, 1.0));
    position = gWorldPositionRoughnessTexture.SampleLevel(gSsaoSampler, uv, 0.0).xyz;
    return depth < 0.9999 && dot(normalMetallic.xyz, normalMetallic.xyz) >= 1e-6;
}

float sampleOcclusion(float2 sampleUv,
                      float3 centerPosition,
                      float3 centerNormal,
                      float radius,
                      float bias)
{
    if (!isInsideScreen(sampleUv))
    {
        return 0.0;
    }

    const float sampleDepth = gDepthTexture.SampleLevel(gSsaoSampler, sampleUv, 0.0);
    if (sampleDepth >= 0.9999)
    {
        return 0.0;
    }

    const float3 samplePosition = gWorldPositionRoughnessTexture.SampleLevel(gSsaoSampler, sampleUv, 0.0).xyz;
    const float3 delta = samplePosition - centerPosition;
    const float distanceToSample = length(delta);
    if (distanceToSample <= 1e-4 || distanceToSample >= radius)
    {
        return 0.0;
    }

    const float3 sampleDirection = delta / distanceToSample;
    const float rangeWeight = saturate(1.0 - distanceToSample / radius);
    const float normalOcclusion = saturate((dot(centerNormal, sampleDirection) - bias) / max(1.0 - bias, 0.001));
    return normalOcclusion * rangeWeight * rangeWeight;
}

[shader("vertex")]
SsaoVertexOutput ssaoVertexMain(uint vertexID : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };

    SsaoVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 ssaoFragmentMain(SsaoVertexOutput input) : SV_Target
{
    const float2 uv = screenUvFromPosition(input.position);
    float depth = 1.0;
    float3 normal = float3(0.0, 0.0, 1.0);
    float3 centerPosition = float3(0.0, 0.0, 0.0);
    if (!loadSurface(uv, depth, normal, centerPosition))
    {
        return float4(1.0, 1.0, 1.0, 1.0);
    }

    const float radius = gSsaoParams.settings.x;
    const float intensity = gSsaoParams.settings.y;
    const float bias = gSsaoParams.settings.z;
    const float power = gSsaoParams.settings.w;
    const float sampleRadiusPixels = clamp(radius * 6.0, 3.0, 18.0);
    const float2 texelRadius = gSsaoParams.framebuffer.xy * sampleRadiusPixels;

    const float2 offsets[16] = {
        float2(1.0, 0.0),
        float2(-1.0, 0.0),
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(0.7071, 0.7071),
        float2(-0.7071, 0.7071),
        float2(0.7071, -0.7071),
        float2(-0.7071, -0.7071),
        float2(1.7, 0.45),
        float2(-1.7, 0.45),
        float2(1.7, -0.45),
        float2(-1.7, -0.45),
        float2(0.45, 1.7),
        float2(-0.45, 1.7),
        float2(0.45, -1.7),
        float2(-0.45, -1.7),
    };

    float occlusion = 0.0;
    [unroll]
    for (int sampleIndex = 0; sampleIndex < 16; ++sampleIndex)
    {
        occlusion += sampleOcclusion(uv + offsets[sampleIndex] * texelRadius,
                                     centerPosition,
                                     normal,
                                     radius,
                                     bias);
    }

    const float ao = pow(saturate(1.0 - (occlusion / 16.0) * intensity), power);
    return float4(ao, ao, ao, 1.0);
}
