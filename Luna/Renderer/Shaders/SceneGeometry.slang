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
    nointerpolation uint pickingId : TEXCOORD5;
};

struct GBufferOutput
{
    float4 baseColorAlpha : SV_Target0;
    float4 normalMetallic : SV_Target1;
    float4 worldPositionRoughness : SV_Target2;
    float4 emissiveAo : SV_Target3;
    uint pickingId : SV_Target4;
};

struct TransparentOutput
{
    float4 color : SV_Target0;
    uint pickingId : SV_Target1;
};

struct MeshPushConstants
{
    float4x4 model;
    uint pickingId;
    uint3 padding;
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
    float4 debugOverlayParams;
    float4 debugPickMarker;
    float4 irradianceSh[9];
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
SamplerState gBaseColorSampler : register(s1);

[[vk::binding(2, 0)]]
Texture2D<float4> gNormalTexture : register(t2);

[[vk::binding(3, 0)]]
SamplerState gNormalSampler : register(s3);

[[vk::binding(4, 0)]]
Texture2D<float4> gMetallicRoughnessTexture : register(t4);

[[vk::binding(5, 0)]]
SamplerState gMetallicRoughnessSampler : register(s5);

[[vk::binding(6, 0)]]
Texture2D<float4> gEmissiveTexture : register(t6);

[[vk::binding(7, 0)]]
SamplerState gEmissiveSampler : register(s7);

[[vk::binding(8, 0)]]
Texture2D<float4> gOcclusionTexture : register(t8);

[[vk::binding(9, 0)]]
SamplerState gOcclusionSampler : register(s9);

[[vk::binding(10, 0)]]
ConstantBuffer<MaterialParams> gMaterialParams : register(b10);

[[vk::binding(0, 1)]]
ConstantBuffer<SceneParams> gSceneParams : register(b11);

[[vk::binding(1, 1)]]
Texture2D<float4> gEnvironmentTexture : register(t12);

[[vk::binding(2, 1)]]
SamplerState gEnvironmentSampler : register(s13);

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

bool isMaskedMaterial()
{
    return gMaterialParams.materialFlags.x > 0.5 && gMaterialParams.materialFlags.x < 1.5;
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
    const float NdotL = saturate(dot(N, L));
    const float NdotV = saturate(dot(N, V));

    const float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    const float3 radiance = gSceneParams.lightColorExposure.rgb * gSceneParams.lightDirectionIntensity.w;

    // When V ~= -L, the microfacet half vector becomes undefined. Treat that direct-specular case as zero
    // instead of falling back to N, otherwise the Fresnel split can produce the black ring + white dot artifact.
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
    const float3 directLighting = directDiffuse + directSpecular;

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

    const float4 baseSample = gBaseColorTexture.Sample(gBaseColorSampler, input.uv);
    material.baseColor = float4(baseSample.rgb * gMaterialParams.baseColorFactor.rgb,
                                baseSample.a * gMaterialParams.baseColorFactor.a);
    if (isMaskedMaterial() && material.baseColor.a < gMaterialParams.materialFactors.w)
    {
        discard;
    }

    const float4 normalSample = gNormalTexture.Sample(gNormalSampler, input.uv);
    const float3 tangentNormal = safeNormalize(float3(normalSample.xy * 2.0 - 1.0, normalSample.z * 2.0 - 1.0),
                                               float3(0.0, 0.0, 1.0));
    const float3x3 tbn = buildTangentFrame(input.worldNormal, input.worldTangent, input.worldBitangent);
    const float3 scaledTangentNormal = safeNormalize(
        float3(tangentNormal.xy * gMaterialParams.emissiveFactorNormalScale.w, tangentNormal.z),
        float3(0.0, 0.0, 1.0));
    material.normalWS = safeNormalize(mul(scaledTangentNormal, tbn), safeNormalize(input.worldNormal, float3(0.0, 0.0, 1.0)));

    const float4 metallicRoughnessSample = gMetallicRoughnessTexture.Sample(gMetallicRoughnessSampler, input.uv);
    material.metallic = saturate(metallicRoughnessSample.b * gMaterialParams.materialFactors.x);
    material.roughness = clamp(metallicRoughnessSample.g * gMaterialParams.materialFactors.y, kMinRoughness, 1.0);

    const float4 emissiveSample = gEmissiveTexture.Sample(gEmissiveSampler, input.uv);
    material.emissive = emissiveSample.rgb * gMaterialParams.emissiveFactorNormalScale.rgb;

    const float occlusionSample = gOcclusionTexture.Sample(gOcclusionSampler, input.uv).r;
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
    output.pickingId = gPushConstants.pickingId;
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
    output.pickingId = input.pickingId;
    return output;
}

[shader("fragment")]
TransparentOutput sceneTransparentFragmentMain(SceneVertexOutput input)
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
    if (gSceneParams.debugOverlayParams.x > 0.5 && input.pickingId != 0)
    {
        color = lerp(color, pickDebugColor(input.pickingId), gSceneParams.debugOverlayParams.y);
    }
    TransparentOutput output;
    output.color = float4(linearToSrgb(toneMapACES(color)), material.baseColor.a);
    output.pickingId = input.pickingId;
    return output;
}
