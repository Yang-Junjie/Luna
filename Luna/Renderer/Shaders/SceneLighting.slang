static const float kPi = 3.14159265359;
static const float kMinRoughness = 0.045;
static const uint kMaxPointLights = 32;
static const uint kMaxSpotLights = 32;

struct SceneLightingVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
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
Texture2D<float4> gGBufferBaseColor : register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> gGBufferNormalMetallic : register(t1);

[[vk::binding(2, 0)]]
Texture2D<float4> gGBufferWorldPositionRoughness : register(t2);

[[vk::binding(3, 0)]]
Texture2D<float4> gGBufferEmissiveAo : register(t3);

[[vk::binding(4, 0)]]
SamplerState gGBufferSampler : register(s4);

[[vk::binding(5, 0)]]
Texture2D<uint> gPickBuffer : register(t5);

[[vk::binding(0, 1)]]
ConstantBuffer<SceneParams> gSceneParams : register(b6);

[[vk::binding(1, 1)]]
Texture2D<float4> gEnvironmentTexture : register(t7);

[[vk::binding(2, 1)]]
SamplerState gEnvironmentSampler : register(s8);

float3 safeNormalize(float3 value, float3 fallbackValue)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 1e-6 ? value * rsqrt(lengthSq) : fallbackValue;
}

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

float2 envBrdfApprox(float roughness, float NdotV);

float3 pickDebugColor(uint pickingId)
{
    const uint hash = pickingId * 1664525u + 1013904223u;
    const float3 hashedColor = float3((hash & 255u) / 255.0,
                                      ((hash >> 8u) & 255u) / 255.0,
                                      ((hash >> 16u) & 255u) / 255.0);
    return lerp(float3(0.20, 0.95, 0.30), hashedColor, 0.45);
}

uint loadPickingId(float2 uv)
{
    uint width = 0;
    uint height = 0;
    gPickBuffer.GetDimensions(width, height);
    if (width == 0 || height == 0)
    {
        return 0;
    }

    const uint2 pixel = min(uint2(uv * float2(width, height)), uint2(width - 1u, height - 1u));
    return gPickBuffer.Load(int3(pixel, 0));
}

bool isDebugPickMarkerPixel(float2 uv)
{
    if (gSceneParams.debugPickMarker.z <= 0.5)
    {
        return false;
    }

    uint width = 0;
    uint height = 0;
    gPickBuffer.GetDimensions(width, height);
    if (width == 0 || height == 0)
    {
        return false;
    }

    const uint2 pixel = min(uint2(uv * float2(width, height)), uint2(width - 1u, height - 1u));
    const int2 debugPixel = int2(gSceneParams.debugPickMarker.xy);
    const int2 delta = abs(int2(pixel) - debugPixel);
    return delta.x <= 2 && delta.y <= 2;
}

bool isPickDebugVisualizationEnabled()
{
    return gSceneParams.debugOverlayParams.x > 0.5;
}

float3 applyPickDebugVisualization(float3 color, uint pickingId, float2 uv)
{
    float3 result = color;
    if (isPickDebugVisualizationEnabled() && pickingId != 0)
    {
        result = lerp(result, pickDebugColor(pickingId), gSceneParams.debugOverlayParams.y);
    }
    if (isDebugPickMarkerPixel(uv))
    {
        result = float3(1.0, 0.15, 0.15);
    }
    return result;
}

float3 evaluateEnvironmentIrradianceSH(float3 direction)
{
    const float3 dir = safeNormalize(direction, float3(0.0, 1.0, 0.0));
    const float x = dir.x;
    const float y = dir.y;
    const float z = dir.z;

    const float basis[9] = {
        0.282095,
        0.488603 * y,
        0.488603 * z,
        0.488603 * x,
        1.092548 * x * y,
        1.092548 * y * z,
        0.315392 * (3.0 * z * z - 1.0),
        1.092548 * x * z,
        0.546274 * (x * x - y * y),
    };

    float3 irradiance = 0.0;
    [unroll]
    for (int basisIndex = 0; basisIndex < 9; ++basisIndex)
    {
        irradiance += gSceneParams.irradianceSh[basisIndex].rgb * basis[basisIndex];
    }
    return max(irradiance, 0.0);
}

float2 directionToEquirectUv(float3 direction)
{
    const float3 dir = safeNormalize(direction, float3(0.0, 1.0, 0.0));
    const float u = atan2(dir.z, dir.x) * (0.5 / kPi) + 0.5;
    const float v = 1.0 - acos(clamp(dir.y, -1.0, 1.0)) / kPi;
    return float2(u, v);
}

float3 sampleEnvironment(float3 direction, float lod)
{
    return gEnvironmentTexture.SampleLevel(gEnvironmentSampler, directionToEquirectUv(direction), lod).rgb;
}

float3 sampleEnvironmentSky(float3 direction, float lod)
{
    return sampleEnvironment(direction, lod);
}

float3 sampleEnvironmentIrradiance(float3 direction)
{
    return evaluateEnvironmentIrradianceSH(direction);
}

float3 sampleEnvironmentPrefilter(float3 direction, float lod)
{
    return sampleEnvironment(direction, lod);
}

float2 sampleEnvironmentBrdf(float roughness, float NdotV)
{
    return envBrdfApprox(roughness, NdotV);
}

float3 reconstructViewDirection(float2 uv)
{
    // This converts fullscreen UVs back into clip-space using the backend's screen-space Y convention.
    const float clipY =
#if defined(LUNA_BACKEND_VULKAN) || defined(LUNA_BACKEND_D3D12)
        uv.y * 2.0 - 1.0;
#else
        1.0 - uv.y * 2.0;
#endif
    const float2 clipXY = float2(uv.x * 2.0 - 1.0, clipY);
    const float4 clipPosition = float4(clipXY, 1.0, 1.0);
    const float4 worldPosition = mul(gSceneParams.inverseViewProjection, clipPosition);
    const float3 worldPoint = worldPosition.xyz / max(abs(worldPosition.w), 1e-6);
    return safeNormalize(worldPoint - gSceneParams.cameraPositionEnvMip.xyz, float3(0.0, 0.0, 1.0));
}

float distributionGGX(float3 N, float3 H, float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float NdotH = saturate(dot(N, H));
    const float NdotH2 = NdotH * NdotH;
    const float denominator = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(kPi * denominator * denominator, 1e-5);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    const float r = roughness + 1.0;
    const float k = (r * r) * 0.125;
    return NdotV / lerp(NdotV, 1.0, k);
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    const float NdotV = saturate(dot(N, V));
    const float NdotL = saturate(dot(N, L));
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) *
                    pow(saturate(1.0 - cosTheta), 5.0);
}

float2 envBrdfApprox(float roughness, float NdotV)
{
    const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
    const float4 r = roughness * c0 + c1;
    const float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    return float2(-1.04, 1.04) * a004 + r.zw;
}

float3 evaluateDirectLight(float3 N,
                           float3 V,
                           float3 L,
                           float3 radiance,
                           float3 baseColor,
                           float metallic,
                           float roughness,
                           float3 F0)
{
    const float NdotL = saturate(dot(N, L));
    const float NdotV = saturate(dot(N, V));

    const float3 directHalfVector = V + L;
    const float directHalfVectorLengthSq = dot(directHalfVector, directHalfVector);
    const bool hasValidDirectHalfVector = NdotL > 0.0 && NdotV > 0.0 && directHalfVectorLengthSq > 1e-6;

    float3 directKs = 0.0;
    float3 directSpecular = 0.0;
    if (hasValidDirectHalfVector)
    {
        const float3 H = directHalfVector * rsqrt(directHalfVectorLengthSq);
        const float HdotV = saturate(dot(H, V));
        const float3 F = fresnelSchlick(HdotV, F0);
        const float NDF = distributionGGX(N, H, roughness);
        const float G = geometrySmith(N, V, L, roughness);
        const float denominator = max(4.0 * NdotL * NdotV, 1e-4);

        directKs = F;
        directSpecular = (NDF * G * F / denominator) * radiance * NdotL;
    }

    const float3 directKd = (1.0 - directKs) * (1.0 - metallic);
    const float3 directDiffuse = directKd * baseColor / kPi * radiance * NdotL;
    return directDiffuse + directSpecular;
}

float calculateRangeAttenuation(float distanceToLight, float range)
{
    const float normalizedDistance = saturate(distanceToLight / max(range, 0.001));
    const float smoothFalloff = saturate(1.0 - normalizedDistance * normalizedDistance);
    return smoothFalloff * smoothFalloff / max(distanceToLight * distanceToLight, 0.01);
}

float3 evaluatePbrLighting(float3 worldPosition,
                           float3 normalWS,
                           float3 baseColor,
                           float metallic,
                           float roughness,
                           float3 emissive,
                           float ao)
{
    const float3 N = safeNormalize(normalWS, float3(0.0, 0.0, 1.0));
    const float3 V = safeNormalize(gSceneParams.cameraPositionEnvMip.xyz - worldPosition, float3(0.0, 0.0, 1.0));
    const float NdotV = saturate(dot(N, V));

    const float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    float3 directLighting = evaluateDirectLight(N,
                                                V,
                                                safeNormalize(gSceneParams.lightDirectionIntensity.xyz, float3(0.0, 1.0, 0.0)),
                                                gSceneParams.lightColorExposure.rgb * gSceneParams.lightDirectionIntensity.w,
                                                baseColor,
                                                metallic,
                                                roughness,
                                                F0);

    const uint pointLightCount = min((uint)gSceneParams.lightCounts.x, kMaxPointLights);
    for (uint lightIndex = 0; lightIndex < pointLightCount; ++lightIndex)
    {
        const float4 positionIntensity = gSceneParams.pointLightPositionIntensity[lightIndex];
        const float4 colorRange = gSceneParams.pointLightColorRange[lightIndex];
        const float3 lightVector = positionIntensity.xyz - worldPosition;
        const float distanceToLight = length(lightVector);
        const float3 pointL = safeNormalize(lightVector, float3(0.0, 1.0, 0.0));
        const float attenuation = calculateRangeAttenuation(distanceToLight, colorRange.w);
        directLighting += evaluateDirectLight(N,
                                             V,
                                             pointL,
                                             colorRange.rgb * positionIntensity.w * attenuation,
                                             baseColor,
                                             metallic,
                                             roughness,
                                             F0);
    }

    const uint spotLightCount = min((uint)gSceneParams.lightCounts.y, kMaxSpotLights);
    for (uint lightIndex = 0; lightIndex < spotLightCount; ++lightIndex)
    {
        const float4 positionIntensity = gSceneParams.spotLightPositionIntensity[lightIndex];
        const float4 directionRange = gSceneParams.spotLightDirectionRange[lightIndex];
        const float4 colorCones = gSceneParams.spotLightColorCones[lightIndex];
        const float4 coneParams = gSceneParams.spotLightConeParams[lightIndex];
        const float3 lightVector = positionIntensity.xyz - worldPosition;
        const float distanceToLight = length(lightVector);
        const float3 spotL = safeNormalize(lightVector, float3(0.0, 1.0, 0.0));
        const float spotCos = dot(-spotL, safeNormalize(directionRange.xyz, float3(0.0, -1.0, 0.0)));
        const float coneAttenuation = saturate((spotCos - coneParams.y) / max(coneParams.x - coneParams.y, 0.001));
        const float attenuation = calculateRangeAttenuation(distanceToLight, directionRange.w) * coneAttenuation * coneAttenuation;
        directLighting += evaluateDirectLight(N,
                                             V,
                                             spotL,
                                             colorCones.rgb * positionIntensity.w * attenuation,
                                             baseColor,
                                             metallic,
                                             roughness,
                                             F0);
    }

    const float maxEnvMip = gSceneParams.cameraPositionEnvMip.w;
    const float3 ambientKs = fresnelSchlickRoughness(NdotV, F0, roughness);
    const float3 ambientKd = (1.0 - ambientKs) * (1.0 - metallic);
    const float3 diffuseIbl = sampleEnvironmentIrradiance(N) * baseColor;

    const float3 R = reflect(-V, N);
    const float3 prefilteredColor = sampleEnvironmentPrefilter(R, roughness * maxEnvMip);
    const float2 envBrdf = sampleEnvironmentBrdf(roughness, NdotV);
    const float3 specularIbl = prefilteredColor * (ambientKs * envBrdf.x + envBrdf.y);
    const float3 ambient = (ambientKd * diffuseIbl * gSceneParams.iblFactors.x + specularIbl * gSceneParams.iblFactors.y) *
                           ao * gSceneParams.iblFactors.z;

    return emissive + directLighting + ambient;
}

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
    const uint pickingId = loadPickingId(input.uv);
    const float4 baseColorAlpha = gGBufferBaseColor.Sample(gGBufferSampler, input.uv);
    const float4 normalMetallic = gGBufferNormalMetallic.Sample(gGBufferSampler, input.uv);
    const float4 worldPositionRoughness = gGBufferWorldPositionRoughness.Sample(gGBufferSampler, input.uv);
    const float4 emissiveAo = gGBufferEmissiveAo.Sample(gGBufferSampler, input.uv);

    if (dot(normalMetallic.xyz, normalMetallic.xyz) < 1e-6 || baseColorAlpha.a <= 0.001)
    {
        const float3 skyDirection = reconstructViewDirection(input.uv);
        float3 skyColor = sampleEnvironmentSky(skyDirection, 0.0);
        skyColor *= gSceneParams.lightColorExposure.a;
        const float3 finalColor = applyPickDebugVisualization(linearToSrgb(toneMapACES(skyColor)), pickingId, input.uv);
        return float4(finalColor, 1.0);
    }

    const float3 baseColor = baseColorAlpha.rgb;
    const float3 normalWS = normalMetallic.xyz;
    const float metallic = saturate(normalMetallic.w);
    const float roughness = clamp(worldPositionRoughness.w, kMinRoughness, 1.0);
    const float3 emissive = emissiveAo.rgb;
    const bool unlit = emissiveAo.a < 0.0;
    const float ao = abs(emissiveAo.a);

    float3 color = baseColor + emissive;
    if (!unlit)
    {
        color = evaluatePbrLighting(worldPositionRoughness.xyz, normalWS, baseColor, metallic, roughness, emissive, ao);
    }

    color *= gSceneParams.lightColorExposure.a;
    const float3 finalColor = applyPickDebugVisualization(linearToSrgb(toneMapACES(color)), pickingId, input.uv);
    return float4(finalColor, baseColorAlpha.a);
}
