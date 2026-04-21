static const float kPi = 3.14159265359;
static const float kMinRoughness = 0.045;

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

[[vk::binding(0, 1)]]
ConstantBuffer<SceneParams> gSceneParams : register(b5);

[[vk::binding(1, 1)]]
Texture2D<float4> gEnvironmentTexture : register(t6);

[[vk::binding(2, 1)]]
SamplerState gEnvironmentSampler : register(s7);

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

float2 directionToEquirectUv(float3 direction)
{
    const float3 dir = safeNormalize(direction, float3(0.0, 1.0, 0.0));
    const float u = atan2(dir.z, dir.x) * (0.5 / kPi) + 0.5;
    // The imported HDR uses a top-origin equirect layout, so +Y must map to the top of the texture.
    const float v = 1.0 - acos(clamp(dir.y, -1.0, 1.0)) / kPi;
    return float2(u, v);
}

float3 sampleEnvironment(float3 direction, float lod)
{
    return gEnvironmentTexture.SampleLevel(gEnvironmentSampler, directionToEquirectUv(direction), lod).rgb;
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
    const float3 L = safeNormalize(gSceneParams.lightDirectionIntensity.xyz, float3(0.0, 1.0, 0.0));
    const float3 H = safeNormalize(V + L, N);
    const float NdotL = saturate(dot(N, L));
    const float NdotV = saturate(dot(N, V));
    const float HdotV = saturate(dot(H, V));

    const float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    const float3 F = fresnelSchlick(HdotV, F0);
    const float NDF = distributionGGX(N, H, roughness);
    const float G = geometrySmith(N, V, L, roughness);
    const float3 numerator = NDF * G * F;
    const float denominator = max(4.0 * NdotL * NdotV, 1e-4);
    const float3 specular = numerator / denominator;

    const float3 kS = F;
    const float3 kD = (1.0 - kS) * (1.0 - metallic);
    const float3 radiance = gSceneParams.lightColorExposure.rgb * gSceneParams.lightDirectionIntensity.w;
    const float3 directLighting = (kD * baseColor / kPi + specular) * radiance * NdotL;

    const float maxEnvMip = gSceneParams.cameraPositionEnvMip.w;
    const float3 irradiance = sampleEnvironment(N, maxEnvMip);
    const float3 diffuseIbl = irradiance * baseColor;

    const float3 R = reflect(-V, N);
    const float3 prefilteredColor = sampleEnvironment(R, roughness * maxEnvMip);
    const float3 Fibl = fresnelSchlickRoughness(NdotV, F0, roughness);
    const float2 envBrdf = envBrdfApprox(roughness, NdotV);
    const float3 specularIbl = prefilteredColor * (Fibl * envBrdf.x + envBrdf.y);
    const float3 ambient = (kD * diffuseIbl * gSceneParams.iblFactors.x + specularIbl * gSceneParams.iblFactors.y) *
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
    const float4 baseColorAlpha = gGBufferBaseColor.Sample(gGBufferSampler, input.uv);
    const float4 normalMetallic = gGBufferNormalMetallic.Sample(gGBufferSampler, input.uv);
    const float4 worldPositionRoughness = gGBufferWorldPositionRoughness.Sample(gGBufferSampler, input.uv);
    const float4 emissiveAo = gGBufferEmissiveAo.Sample(gGBufferSampler, input.uv);

    if (dot(normalMetallic.xyz, normalMetallic.xyz) < 1e-6 || baseColorAlpha.a <= 0.001)
    {
        const float3 skyDirection = reconstructViewDirection(input.uv);
        float3 skyColor = sampleEnvironment(skyDirection, 0.0);
        skyColor *= gSceneParams.lightColorExposure.a;
        return float4(linearToSrgb(toneMapACES(skyColor)), 1.0);
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
    return float4(linearToSrgb(toneMapACES(color)), baseColorAlpha.a);
}
