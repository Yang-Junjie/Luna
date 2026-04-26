static const float kPi = 3.14159265359;

struct IblDispatchParams
{
    uint outputSize;
    uint sampleCount;
    float roughness;
    uint reserved;
};

#if defined(LUNA_BACKEND_D3D11)
[[vk::push_constant]]
cbuffer PushConstants : register(b13)
{
    IblDispatchParams gDispatchParams;
}
#elif defined(LUNA_BACKEND_D3D12)
[[vk::push_constant]]
cbuffer PushConstants : register(b15)
{
    IblDispatchParams gDispatchParams;
}
#else
[[vk::push_constant]]
cbuffer PushConstants
{
    IblDispatchParams gDispatchParams;
}
#endif

[[vk::binding(0, 0)]]
Texture2D<float4> gEnvironmentEquirectTexture : register(t0);

[[vk::binding(1, 0)]]
SamplerState gEnvironmentSampler : register(s1);

[[vk::binding(2, 0)]]
RWTexture2DArray<float4> gCubeOutputTexture : register(u2);

[[vk::binding(3, 0)]]
TextureCube<float4> gEnvironmentCubeTexture : register(t3);

[[vk::binding(4, 0)]]
RWTexture2D<float4> gBrdfLutTexture : register(u4);

float3 safeNormalize(float3 value, float3 fallbackValue)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 1e-6 ? value * rsqrt(lengthSq) : fallbackValue;
}

float2 directionToEquirectUv(float3 direction)
{
    const float3 dir = safeNormalize(direction, float3(0.0, 1.0, 0.0));
    const float u = atan2(dir.z, dir.x) * (0.5 / kPi) + 0.5;
    const float v = 1.0 - acos(clamp(dir.y, -1.0, 1.0)) / kPi;
    return float2(u, v);
}

float3 sampleEnvironmentSource(float3 direction)
{
    return gEnvironmentEquirectTexture.SampleLevel(gEnvironmentSampler, directionToEquirectUv(direction), 0.0).rgb;
}

float3 sampleEnvironmentCube(float3 direction)
{
    return gEnvironmentCubeTexture.SampleLevel(gEnvironmentSampler, safeNormalize(direction, float3(0.0, 1.0, 0.0)), 0.0)
        .rgb;
}

float3 cubemapDirectionFromTexel(uint faceIndex, uint2 pixelCoord, uint faceSize)
{
    const float2 uv = ((float2(pixelCoord) + 0.5) / max(float(faceSize), 1.0)) * 2.0 - 1.0;
    const float x = uv.x;
    const float y = uv.y;

    switch (faceIndex)
    {
    case 0:
        return safeNormalize(float3(1.0, -y, -x), float3(1.0, 0.0, 0.0));
    case 1:
        return safeNormalize(float3(-1.0, -y, x), float3(-1.0, 0.0, 0.0));
    case 2:
        return safeNormalize(float3(x, 1.0, y), float3(0.0, 1.0, 0.0));
    case 3:
        return safeNormalize(float3(x, -1.0, -y), float3(0.0, -1.0, 0.0));
    case 4:
        return safeNormalize(float3(x, -y, 1.0), float3(0.0, 0.0, 1.0));
    default:
        return safeNormalize(float3(-x, -y, -1.0), float3(0.0, 0.0, -1.0));
    }
}

float radicalInverseVdC(uint bits)
{
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    return float(bits) * 2.3283064365386963e-10;
}

float2 hammersley(uint sampleIndex, uint sampleCount)
{
    return float2(float(sampleIndex) / max(float(sampleCount), 1.0), radicalInverseVdC(sampleIndex));
}

float3 buildWorldVector(float3 N, float3 tangentSpaceVector)
{
    const float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    const float3 tangent = safeNormalize(cross(up, N), float3(1.0, 0.0, 0.0));
    const float3 bitangent = cross(N, tangent);
    return tangent * tangentSpaceVector.x + bitangent * tangentSpaceVector.y + N * tangentSpaceVector.z;
}

float3 cosineSampleHemisphere(float2 xi)
{
    const float phi = 2.0 * kPi * xi.x;
    const float cosTheta = sqrt(1.0 - xi.y);
    const float sinTheta = sqrt(xi.y);
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3 importanceSampleGGX(float2 xi, float roughness, float3 N)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float phi = 2.0 * kPi * xi.x;
    const float cosTheta = sqrt((1.0 - xi.y) / max(1.0 + (a2 - 1.0) * xi.y, 1e-4));
    const float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));

    const float3 Ht = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    return safeNormalize(buildWorldVector(N, Ht), N);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    const float r = roughness + 1.0;
    const float k = (r * r) * 0.125;
    return NdotV / lerp(NdotV, 1.0, k);
}

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

float2 integrateBrdf(float NdotV, float roughness, uint sampleCount)
{
    const float3 N = float3(0.0, 0.0, 1.0);
    float3 V = float3(sqrt(saturate(1.0 - NdotV * NdotV)), 0.0, NdotV);

    float A = 0.0;
    float B = 0.0;

    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        const float2 xi = hammersley(sampleIndex, sampleCount);
        const float3 H = importanceSampleGGX(xi, roughness, N);
        const float3 L = safeNormalize(2.0 * dot(V, H) * H - V, N);

        const float NdotL = saturate(L.z);
        const float NdotH = saturate(H.z);
        const float VdotH = saturate(dot(V, H));

        if (NdotL > 0.0)
        {
            const float G = geometrySmith(NdotV, NdotL, roughness);
            const float GVis = (G * VdotH) / max(NdotH * NdotV, 1e-4);
            const float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * GVis;
            B += Fc * GVis;
        }
    }

    return float2(A, B) / max(float(sampleCount), 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void environmentEquirectToCubeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint faceSize = gDispatchParams.outputSize;
    if (dispatchThreadID.x >= faceSize || dispatchThreadID.y >= faceSize || dispatchThreadID.z >= 6)
    {
        return;
    }

    const float3 direction = cubemapDirectionFromTexel(dispatchThreadID.z, dispatchThreadID.xy, faceSize);
    const float3 color = sampleEnvironmentSource(direction);
    gCubeOutputTexture[dispatchThreadID] = float4(color, 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void environmentIrradianceMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint faceSize = gDispatchParams.outputSize;
    const uint sampleCount = max(gDispatchParams.sampleCount, 1u);
    if (dispatchThreadID.x >= faceSize || dispatchThreadID.y >= faceSize || dispatchThreadID.z >= 6)
    {
        return;
    }

    const float3 N = cubemapDirectionFromTexel(dispatchThreadID.z, dispatchThreadID.xy, faceSize);
    float3 irradiance = 0.0;

    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        const float2 xi = hammersley(sampleIndex, sampleCount);
        const float3 sampleVector = buildWorldVector(N, cosineSampleHemisphere(xi));
        irradiance += sampleEnvironmentCube(sampleVector);
    }

    irradiance *= kPi / max(float(sampleCount), 1.0);
    gCubeOutputTexture[dispatchThreadID] = float4(irradiance, 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void environmentPrefilterMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint faceSize = gDispatchParams.outputSize;
    const uint sampleCount = max(gDispatchParams.sampleCount, 1u);
    const float roughness = saturate(gDispatchParams.roughness);
    if (dispatchThreadID.x >= faceSize || dispatchThreadID.y >= faceSize || dispatchThreadID.z >= 6)
    {
        return;
    }

    const float3 R = cubemapDirectionFromTexel(dispatchThreadID.z, dispatchThreadID.xy, faceSize);
    const float3 N = R;
    const float3 V = R;

    float3 prefilteredColor = 0.0;
    float totalWeight = 0.0;

    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        const float2 xi = hammersley(sampleIndex, sampleCount);
        const float3 H = importanceSampleGGX(xi, roughness, N);
        const float3 L = safeNormalize(2.0 * dot(V, H) * H - V, N);
        const float NdotL = saturate(dot(N, L));

        if (NdotL > 0.0)
        {
            prefilteredColor += sampleEnvironmentCube(L) * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor /= max(totalWeight, 1e-4);
    gCubeOutputTexture[dispatchThreadID] = float4(prefilteredColor, 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void environmentBrdfLutMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint lutSize = gDispatchParams.outputSize;
    const uint sampleCount = max(gDispatchParams.sampleCount, 1u);
    if (dispatchThreadID.x >= lutSize || dispatchThreadID.y >= lutSize)
    {
        return;
    }

    const float2 uv = (float2(dispatchThreadID.xy) + 0.5) / max(float(lutSize), 1.0);
    const float NdotV = saturate(uv.x);
    const float roughness = saturate(uv.y);
    const float2 brdf = integrateBrdf(NdotV, roughness, sampleCount);
    gBrdfLutTexture[dispatchThreadID.xy] = float4(brdf, 0.0, 1.0);
}
