static const float kPi = 3.14159265359;
static const float kMinRoughness = 0.045;

struct SceneVertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
};

struct SceneVertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
    float3 worldTangent : TEXCOORD3;
    float3 worldBitangent : TEXCOORD4;
};

struct GBufferOutput
{
    float4 baseColorAlpha : SV_Target0;
    float4 normalMetallic : SV_Target1;
    float4 worldPositionRoughness : SV_Target2;
    float4 emissiveAo : SV_Target3;
};

struct MeshPushConstants
{
    float4x4 model;
};

struct MaterialParams
{
    float4 baseColorFactor;
    float4 emissiveFactorNormalScale;
    float4 materialFactors;
    float4 materialFlags;
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

struct SampledMaterial
{
    float4 baseColor;
    float3 normalWS;
    float metallic;
    float roughness;
    float3 emissive;
    float ao;
    float unlit;
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
Texture2D<float4> gBaseColorTexture : register(t0);

[[vk::binding(1, 0)]]
SamplerState gMaterialSampler : register(s1);

[[vk::binding(2, 0)]]
Texture2D<float4> gNormalTexture : register(t2);

[[vk::binding(3, 0)]]
Texture2D<float4> gMetallicRoughnessTexture : register(t3);

[[vk::binding(4, 0)]]
Texture2D<float4> gEmissiveTexture : register(t4);

[[vk::binding(5, 0)]]
Texture2D<float4> gOcclusionTexture : register(t5);

[[vk::binding(6, 0)]]
ConstantBuffer<MaterialParams> gMaterialParams : register(b6);

[[vk::binding(0, 1)]]
ConstantBuffer<SceneParams> gSceneParams : register(b7);

[[vk::binding(1, 1)]]
Texture2D<float4> gEnvironmentTexture : register(t8);

[[vk::binding(2, 1)]]
SamplerState gEnvironmentSampler : register(s9);

float3 safeNormalize(float3 value, float3 fallbackValue)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 1e-6 ? value * rsqrt(lengthSq) : fallbackValue;
}

float3 srgbToLinear(float3 color)
{
    return pow(saturate(color), 2.2);
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

bool isMaskedMaterial()
{
    return gMaterialParams.materialFlags.x > 0.5 && gMaterialParams.materialFlags.x < 1.5;
}

float2 directionToEquirectUv(float3 direction)
{
    const float3 dir = safeNormalize(direction, float3(0.0, 1.0, 0.0));
    const float u = atan2(dir.z, dir.x) * (0.5 / kPi) + 0.5;
    const float v = acos(clamp(dir.y, -1.0, 1.0)) / kPi;
    return float2(u, v);
}

float3 sampleEnvironment(float3 direction, float lod)
{
    return gEnvironmentTexture.SampleLevel(gEnvironmentSampler, directionToEquirectUv(direction), lod).rgb;
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

float3x3 buildTangentFrame(float3 worldNormal, float3 worldTangent, float3 worldBitangent)
{
    const float3 N = safeNormalize(worldNormal, float3(0.0, 0.0, 1.0));

    float3 T = worldTangent;
    if (dot(T, T) < 1e-6)
    {
        const float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
        T = cross(up, N);
    }
    T = safeNormalize(T - N * dot(T, N), float3(1.0, 0.0, 0.0));

    float handedness = dot(cross(N, T), worldBitangent) < 0.0 ? -1.0 : 1.0;
    const float3 B = safeNormalize(cross(N, T) * handedness, float3(0.0, 1.0, 0.0));
    return float3x3(T, B, N);
}

SampledMaterial sampleMaterial(SceneVertexOutput input)
{
    SampledMaterial material;

    const float4 baseSample = gBaseColorTexture.Sample(gMaterialSampler, input.uv);
    material.baseColor = float4(srgbToLinear(baseSample.rgb) * gMaterialParams.baseColorFactor.rgb,
                                baseSample.a * gMaterialParams.baseColorFactor.a);
    if (isMaskedMaterial() && material.baseColor.a < gMaterialParams.materialFactors.w)
    {
        discard;
    }

    const float4 normalSample = gNormalTexture.Sample(gMaterialSampler, input.uv);
    const float3 tangentNormal = safeNormalize(float3(normalSample.xy * 2.0 - 1.0, normalSample.z * 2.0 - 1.0),
                                               float3(0.0, 0.0, 1.0));
    const float3x3 tbn = buildTangentFrame(input.worldNormal, input.worldTangent, input.worldBitangent);
    const float3 scaledTangentNormal = safeNormalize(
        float3(tangentNormal.xy * gMaterialParams.emissiveFactorNormalScale.w, tangentNormal.z),
        float3(0.0, 0.0, 1.0));
    material.normalWS = safeNormalize(mul(scaledTangentNormal, tbn), safeNormalize(input.worldNormal, float3(0.0, 0.0, 1.0)));

    const float4 metallicRoughnessSample = gMetallicRoughnessTexture.Sample(gMaterialSampler, input.uv);
    material.metallic = saturate(metallicRoughnessSample.b * gMaterialParams.materialFactors.x);
    material.roughness = clamp(metallicRoughnessSample.g * gMaterialParams.materialFactors.y, kMinRoughness, 1.0);

    const float4 emissiveSample = gEmissiveTexture.Sample(gMaterialSampler, input.uv);
    material.emissive = srgbToLinear(emissiveSample.rgb) * gMaterialParams.emissiveFactorNormalScale.rgb;

    const float occlusionSample = gOcclusionTexture.Sample(gMaterialSampler, input.uv).r;
    material.ao = lerp(1.0, occlusionSample, saturate(gMaterialParams.materialFactors.z));
    material.unlit = gMaterialParams.materialFlags.y;
    return material;
}

[shader("vertex")]
SceneVertexOutput sceneGeometryVertexMain(SceneVertexInput input)
{
    SceneVertexOutput output;
    const float4 worldPosition = mul(gPushConstants.model, float4(input.position, 1.0));
    const float3x3 normalMatrix = (float3x3)gPushConstants.model;

    output.position = mul(gSceneParams.viewProjection, worldPosition);
    output.worldPosition = worldPosition.xyz;
    output.uv = input.uv;
    output.worldNormal = mul(normalMatrix, input.normal);
    output.worldTangent = mul(normalMatrix, input.tangent);
    output.worldBitangent = mul(normalMatrix, input.bitangent);
    return output;
}

[shader("fragment")]
GBufferOutput sceneGeometryFragmentMain(SceneVertexOutput input)
{
    const SampledMaterial material = sampleMaterial(input);

    GBufferOutput output;
    output.baseColorAlpha = material.baseColor;
    output.normalMetallic = float4(material.normalWS, material.metallic);
    output.worldPositionRoughness = float4(input.worldPosition, material.roughness);
    output.emissiveAo = float4(material.emissive, material.unlit > 0.5 ? -material.ao : material.ao);
    return output;
}

[shader("fragment")]
float4 sceneTransparentFragmentMain(SceneVertexOutput input) : SV_Target
{
    const SampledMaterial material = sampleMaterial(input);
    if (material.baseColor.a <= 0.001)
    {
        discard;
    }

    float3 color = material.baseColor.rgb + material.emissive;
    if (material.unlit <= 0.5)
    {
        color = evaluatePbrLighting(input.worldPosition,
                                    material.normalWS,
                                    material.baseColor.rgb,
                                    material.metallic,
                                    material.roughness,
                                    material.emissive,
                                    material.ao);
    }

    color *= gSceneParams.lightColorExposure.a;
    return float4(linearToSrgb(toneMapACES(color)), material.baseColor.a);
}
