static const float kPi = 3.14159265359;
static const float kTwoPi = 6.28318530718;

struct ProceduralSkyParams
{
    float4 sunDirectionIntensity;
    float4 skyColorZenithExposure;
    float4 skyColorHorizonSunRadius;
    float4 groundColor;
    uint4 outputSize;
};

#if defined(LUNA_BACKEND_D3D11)
[[vk::push_constant]]
cbuffer PushConstants : register(b13)
{
    ProceduralSkyParams gSkyParams;
}
#elif defined(LUNA_BACKEND_D3D12)
[[vk::push_constant]]
cbuffer PushConstants : register(b15)
{
    ProceduralSkyParams gSkyParams;
}
#else
[[vk::push_constant]]
cbuffer PushConstants
{
    ProceduralSkyParams gSkyParams;
}
#endif

[[vk::binding(0, 0)]]
RWTexture2D<float4> gSkyEquirectOutput : register(u0);

float saturateFloat(float value)
{
    return clamp(value, 0.0, 1.0);
}

float smoothStepFloat(float edge0, float edge1, float value)
{
    const float t = saturateFloat((value - edge0) / max(edge1 - edge0, 1e-5));
    return t * t * (3.0 - 2.0 * t);
}

float3 safeNormalize(float3 value, float3 fallbackValue)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 1e-6 ? value * rsqrt(lengthSq) : fallbackValue;
}

float3 computeRayleighScattering(float3 direction, float3 zenithColor, float3 horizonColor)
{
    const float height = max(direction.y, 0.0);
    const float zenithFactor = pow(height, 0.5);
    float3 skyColor = lerp(horizonColor, zenithColor, zenithFactor);
    const float horizonGlow = exp(-height * 4.0) * 0.3;
    skyColor += horizonColor * horizonGlow;
    return skyColor;
}

float3 computeMieScattering(float3 direction, float3 sunDir)
{
    const float cosTheta = dot(direction, sunDir);
    const float g = 0.76;
    const float g2 = g * g;
    const float phase = (1.0 - g2) / (4.0 * kPi * pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-5), 1.5));
    return float3(1.0, 0.9, 0.7) * phase * 0.15;
}

float3 computeSunDisc(float3 direction, float3 sunDir, float sunAngularRadius, float sunIntensity)
{
    const float radius = max(sunAngularRadius, 0.0001);
    const float cosTheta = dot(direction, sunDir);
    const float sunCosAngle = cos(radius);
    const float edgeSoftness = radius * 0.3;
    const float sunDisc = smoothStepFloat(cos(radius + edgeSoftness), sunCosAngle, cosTheta);
    return float3(1.0, 0.95, 0.9) * max(sunIntensity, 0.0) * sunDisc;
}

float3 computeProceduralSky(float3 direction)
{
    const float3 dir = safeNormalize(direction, float3(0.0, 1.0, 0.0));
    const float3 sunDir = safeNormalize(gSkyParams.sunDirectionIntensity.xyz, float3(0.51214755, 0.76822126, 0.38411063));
    const float exposure = max(gSkyParams.skyColorZenithExposure.w, 0.0);
    const float3 zenithColor = max(gSkyParams.skyColorZenithExposure.xyz, 0.0);
    const float3 horizonColor = max(gSkyParams.skyColorHorizonSunRadius.xyz, 0.0);
    const float3 groundColor = max(gSkyParams.groundColor.xyz, 0.0);

    if (dir.y > -0.02)
    {
        float3 skyColor = computeRayleighScattering(dir, zenithColor, horizonColor);
        skyColor += computeMieScattering(dir, sunDir);

        if (sunDir.y > 0.0)
        {
            skyColor += computeSunDisc(dir,
                                       sunDir,
                                       gSkyParams.skyColorHorizonSunRadius.w,
                                       gSkyParams.sunDirectionIntensity.w);
        }

        const float horizonBlend = smoothStepFloat(-0.02, 0.05, dir.y);
        const float3 sky = skyColor * exposure;
        return max(lerp(groundColor * exposure * 0.5, sky, horizonBlend), 0.0);
    }

    const float groundFade = smoothStepFloat(-1.0, -0.1, dir.y);
    return max(groundColor * exposure * 0.5 * groundFade, 0.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void proceduralSkyEquirectMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint width = max(gSkyParams.outputSize.x, 1u);
    const uint height = max(gSkyParams.outputSize.y, 1u);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
    {
        return;
    }

    const float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(width, height);
    const float theta = uv.y * kPi;
    const float phi = (uv.x - 0.5) * kTwoPi;
    const float sinTheta = sin(theta);
    const float3 direction = safeNormalize(float3(cos(phi) * sinTheta, cos(theta), sin(phi) * sinTheta),
                                           float3(0.0, 1.0, 0.0));

    gSkyEquirectOutput[dispatchThreadID.xy] = float4(computeProceduralSky(direction), 1.0);
}
