struct GridVertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct GridParams {
    float4x4 viewProjection;
    float4x4 inverseViewProjection;
    float4 cameraPositionFadeDistance;
    float4 framebuffer;
    float4 settings;
    float4 thinColor;
    float4 thickColor;
    float4 axisColorX;
    float4 axisColorZ;
};

struct GridFragmentOutput {
    float4 color : SV_Target;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GridParams> gGridParams : register(b0);

[[vk::binding(1, 0)]]
Texture2D<float4> gGBufferBaseColor : register(t1);

[[vk::binding(2, 0)]]
Texture2D<float4> gGBufferNormalMetallic : register(t2);

[[vk::binding(3, 0)]]
Texture2D<float4> gGBufferWorldPositionRoughness : register(t3);

[[vk::binding(4, 0)]]
SamplerState gGBufferSampler : register(s4);

float2 clipFromUv(float2 uv)
{
    const float clipY =
#if defined(LUNA_BACKEND_VULKAN) || defined(LUNA_BACKEND_D3D12)
        uv.y * 2.0 - 1.0;
#else
        1.0 - uv.y * 2.0;
#endif
    return float2(uv.x * 2.0 - 1.0, clipY);
}

float3 unprojectPoint(float2 uv, float depth)
{
    const float2 clipXY = clipFromUv(uv);
    const float4 worldPosition = mul(gGridParams.inverseViewProjection, float4(clipXY, depth, 1.0));
    const float invW = abs(worldPosition.w) > 1.0e-6 ? 1.0 / worldPosition.w : 0.0;
    return worldPosition.xyz * invW;
}

float computeDepth(float3 worldPosition)
{
    const float4 clipPosition = mul(gGridParams.viewProjection, float4(worldPosition, 1.0));
    const float invW = abs(clipPosition.w) > 1.0e-6 ? 1.0 / clipPosition.w : 0.0;
    return clipPosition.z * invW;
}

float pristineGridLine(float2 uv)
{
    const float2 dudv = max(fwidth(uv), float2(1.0e-5, 1.0e-5));
    const float2 uvMod = frac(uv);
    const float2 uvDist = min(uvMod, 1.0 - uvMod);
    const float2 distInPixels = uvDist / dudv;
    const float2 lineAlpha = 1.0 - smoothstep(float2(0.0, 0.0), float2(1.0, 1.0), distInPixels);
    const float density = max(dudv.x, dudv.y);
    const float densityFade = 1.0 - smoothstep(0.5, 1.0, density);
    return max(lineAlpha.x, lineAlpha.y) * densityFade;
}

float axisLineAA(float coord, float dudv)
{
    const float distInPixels = abs(coord) / max(dudv, 1.0e-5);
    return 1.0 - smoothstep(0.0, 1.5, distInPixels);
}

bool isGBufferSurface(float4 baseColorAlpha, float4 normalMetallic)
{
    return dot(normalMetallic.xyz, normalMetallic.xyz) >= 1.0e-6 && baseColorAlpha.a > 0.001;
}

bool sceneSurfaceOccludesGrid(float2 uv, float3 nearPoint, float3 ray, float gridRayT)
{
    const float4 baseColorAlpha = gGBufferBaseColor.SampleLevel(gGBufferSampler, uv, 0.0);
    const float4 normalMetallic = gGBufferNormalMetallic.SampleLevel(gGBufferSampler, uv, 0.0);
    if (!isGBufferSurface(baseColorAlpha, normalMetallic)) {
        return false;
    }

    const float3 sceneWorldPosition = gGBufferWorldPositionRoughness.SampleLevel(gGBufferSampler, uv, 0.0).xyz;
    const float rayLengthSq = dot(ray, ray);
    if (rayLengthSq <= 1.0e-8) {
        return false;
    }

    const float sceneRayT = dot(sceneWorldPosition - nearPoint, ray) / rayLengthSq;
    return sceneRayT >= 0.0 && gridRayT >= sceneRayT - 1.0e-4;
}

[shader("vertex")]
GridVertexOutput editorInfiniteGridVertexMain(uint vertexID: SV_VertexID)
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

    GridVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.uv = uvs[vertexID];
    return output;
}

[shader("fragment")]
GridFragmentOutput editorInfiniteGridFragmentMain(GridVertexOutput input)
{
    const float2 uv = input.uv;
    const float3 nearPoint = unprojectPoint(uv, 0.0);
    const float3 farPoint = unprojectPoint(uv, 1.0);
    const float3 ray = farPoint - nearPoint;
    if (abs(ray.y) < 1.0e-5) {
        discard;
    }

    const float t = -nearPoint.y / ray.y;
    if (t < 0.0) {
        discard;
    }

    const float3 worldPosition = nearPoint + t * ray;
    const float depth = computeDepth(worldPosition);
    if (depth < 0.0 || depth > 1.0) {
        discard;
    }
    if (sceneSurfaceOccludesGrid(uv, nearPoint, ray, t)) {
        discard;
    }

    const float2 worldGridPosition = worldPosition.xz;
    const float gridScale = max(gGridParams.settings.x, 1.0e-4);
    const float opacity = saturate(gGridParams.settings.y);
    const float fadeDistance = max(gGridParams.cameraPositionFadeDistance.w, 1.0);

    const float distanceToCamera = length(worldPosition - gGridParams.cameraPositionFadeDistance.xyz);
    const float radialFade = 1.0 - smoothstep(fadeDistance * 0.3, fadeDistance, distanceToCamera);
    const float3 viewDirection = normalize(worldPosition - gGridParams.cameraPositionFadeDistance.xyz);
    const float normalFade = smoothstep(0.0, 0.15, abs(viewDirection.y));
    const float fade = radialFade * normalFade * opacity;
    if (fade < 0.001) {
        discard;
    }

    const float2 gridCoord1 = worldGridPosition / gridScale;
    const float2 gridCoord10 = worldGridPosition / (gridScale * 10.0);
    const float minorGrid = pristineGridLine(gridCoord1);
    const float majorGrid = pristineGridLine(gridCoord10);
    const float2 deriv1 = fwidth(gridCoord1);
    const float lodFactor = smoothstep(0.3, 0.6, max(deriv1.x, deriv1.y));

    const float minorAlpha = minorGrid * gGridParams.thinColor.a;
    const float majorAlpha = majorGrid * gGridParams.thickColor.a;
    const float nearGridAlpha = minorAlpha + majorAlpha;
    const float farGridAlpha = majorAlpha;
    const float gridAlpha = lerp(nearGridAlpha, farGridAlpha, lodFactor) * fade;

    const float majorWeight = majorAlpha / max(minorAlpha + majorAlpha, 1.0e-5);
    float3 finalColor = lerp(gGridParams.thinColor.rgb, gGridParams.thickColor.rgb, saturate(majorWeight));
    float finalAlpha = gridAlpha;

    const float2 worldDeriv = max(fwidth(worldGridPosition), float2(1.0e-5, 1.0e-5));
    const float zAxisAlpha = axisLineAA(worldPosition.z, worldDeriv.y) * fade;
    const float xAxisAlpha = axisLineAA(worldPosition.x, worldDeriv.x) * fade;

    const float zAxisBlend = zAxisAlpha * gGridParams.axisColorX.a;
    if (zAxisBlend > 0.001) {
        finalColor = lerp(finalColor, gGridParams.axisColorX.rgb, saturate(zAxisBlend));
        finalAlpha = max(finalAlpha, zAxisBlend);
    }

    const float xAxisBlend = xAxisAlpha * gGridParams.axisColorZ.a;
    if (xAxisBlend > 0.001) {
        finalColor = lerp(finalColor, gGridParams.axisColorZ.rgb, saturate(xAxisBlend));
        finalAlpha = max(finalAlpha, xAxisBlend);
    }

    if (finalAlpha < 0.001) {
        discard;
    }

    GridFragmentOutput output;
    output.color = float4(finalColor, saturate(finalAlpha));
    return output;
}
