struct TaaVertexOutput
{
    float4 position : SV_Position;
};

struct TaaParams
{
    float4 framebuffer;
    float4 settings;
    float4 jitterPixels;
};

struct TaaFragmentOutput
{
    float4 color : SV_Target0;
    float4 history : SV_Target1;
};

[[vk::binding(0, 0)]]
Texture2D<float4> gCurrentColorTexture : register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> gHistoryColorTexture : register(t1);

[[vk::binding(2, 0)]]
Texture2D<float2> gVelocityTexture : register(t2);

[[vk::binding(3, 0)]]
Texture2D<float> gDepthTexture : register(t3);

[[vk::binding(4, 0)]]
SamplerState gTaaSampler : register(s4);

[[vk::binding(5, 0)]]
ConstantBuffer<TaaParams> gTaaParams : register(b5);

float2 screenUvFromPosition(float4 position)
{
    return saturate(position.xy * gTaaParams.framebuffer.xy);
}

bool isInsideScreen(float2 uv)
{
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

bool hasSurfaceDepth(float depth)
{
    return depth < 0.9999;
}

float luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 rgbToYCoCg(float3 color)
{
    return float3(color.r * 0.25 + color.g * 0.5 + color.b * 0.25,
                  color.r - color.b,
                  color.g - color.r * 0.5 - color.b * 0.5);
}

float3 yCoCgToRgb(float3 color)
{
    const float tmp = color.x - color.z * 0.5;
    return float3(tmp + color.y * 0.5,
                  color.x + color.z * 0.5,
                  tmp - color.y * 0.5);
}

float4 sampleCurrentColor(float2 uv)
{
    return gCurrentColorTexture.SampleLevel(gTaaSampler, saturate(uv), 0.0);
}

int2 framebufferSize()
{
    return int2(int(gTaaParams.framebuffer.z), int(gTaaParams.framebuffer.w));
}

int2 clampPixel(int2 pixel)
{
    return clamp(pixel, int2(0, 0), framebufferSize() - int2(1, 1));
}

int2 pixelFromUv(float2 uv)
{
    return clampPixel(int2(uv * gTaaParams.framebuffer.zw));
}

float4 loadCurrentColor(int2 pixel)
{
    const int2 clampedPixel = clampPixel(pixel);
    return gCurrentColorTexture.Load(int3(clampedPixel.x, clampedPixel.y, 0));
}

float loadDepth(int2 pixel)
{
    const int2 clampedPixel = clampPixel(pixel);
    return gDepthTexture.Load(int3(clampedPixel.x, clampedPixel.y, 0));
}

float2 loadVelocity(int2 pixel)
{
    const int2 clampedPixel = clampPixel(pixel);
    return gVelocityTexture.Load(int3(clampedPixel.x, clampedPixel.y, 0));
}

float4 loadHistoryColor(int2 pixel)
{
    const int2 clampedPixel = clampPixel(pixel);
    return gHistoryColorTexture.Load(int3(clampedPixel.x, clampedPixel.y, 0));
}

float4 sampleHistoryCatmullRom(float2 uv)
{
    const float2 samplePosition = uv * gTaaParams.framebuffer.zw - 0.5;
    const int2 basePixel = int2(floor(samplePosition));
    const float2 f = frac(samplePosition);

    const float4 wx = float4(
        f.x * (-0.5 + f.x * (1.0 - 0.5 * f.x)),
        1.0 + f.x * f.x * (-2.5 + 1.5 * f.x),
        f.x * (0.5 + f.x * (2.0 - 1.5 * f.x)),
        f.x * f.x * (-0.5 + 0.5 * f.x));
    const float4 wy = float4(
        f.y * (-0.5 + f.y * (1.0 - 0.5 * f.y)),
        1.0 + f.y * f.y * (-2.5 + 1.5 * f.y),
        f.y * (0.5 + f.y * (2.0 - 1.5 * f.y)),
        f.y * f.y * (-0.5 + 0.5 * f.y));

    float4 color = float4(0.0);
    [unroll]
    for (int y = 0; y < 4; ++y)
    {
        [unroll]
        for (int x = 0; x < 4; ++x)
        {
            color += loadHistoryColor(basePixel + int2(x - 1, y - 1)) * wx[x] * wy[y];
        }
    }
    return max(color, float4(0.0));
}

void chooseClosestDepthPixel(int2 centerPixel, int2 pixelOffset, inout float closestDepth, inout int2 closestPixel)
{
    const int2 samplePixel = clampPixel(centerPixel + pixelOffset);
    const float sampleDepth = loadDepth(samplePixel);
    if (sampleDepth < closestDepth)
    {
        closestDepth = sampleDepth;
        closestPixel = samplePixel;
    }
}

float2 loadClosestDepthVelocity(int2 centerPixel, float centerDepth)
{
    int2 closestPixel = clampPixel(centerPixel);
    float closestDepth = centerDepth;

    chooseClosestDepthPixel(centerPixel, int2(-1, -1), closestDepth, closestPixel);
    chooseClosestDepthPixel(centerPixel, int2(0, -1), closestDepth, closestPixel);
    chooseClosestDepthPixel(centerPixel, int2(1, -1), closestDepth, closestPixel);
    chooseClosestDepthPixel(centerPixel, int2(-1, 0), closestDepth, closestPixel);
    chooseClosestDepthPixel(centerPixel, int2(1, 0), closestDepth, closestPixel);
    chooseClosestDepthPixel(centerPixel, int2(-1, 1), closestDepth, closestPixel);
    chooseClosestDepthPixel(centerPixel, int2(0, 1), closestDepth, closestPixel);
    chooseClosestDepthPixel(centerPixel, int2(1, 1), closestDepth, closestPixel);

    return loadVelocity(closestPixel);
}

void accumulateVelocityDeltaPixels(int2 centerPixel,
                                   int2 pixelOffset,
                                   float2 centerVelocity,
                                   inout float maxVelocityDeltaPixels)
{
    const float2 sampleVelocity = loadVelocity(centerPixel + pixelOffset);
    const float velocityDeltaPixels = length((sampleVelocity - centerVelocity) * gTaaParams.framebuffer.zw);
    maxVelocityDeltaPixels = max(maxVelocityDeltaPixels, velocityDeltaPixels);
}

float velocityNeighborhoodDeltaPixels(int2 centerPixel, float2 centerVelocity)
{
    float maxVelocityDeltaPixels = 0.0;
    accumulateVelocityDeltaPixels(centerPixel, int2(-1, 0), centerVelocity, maxVelocityDeltaPixels);
    accumulateVelocityDeltaPixels(centerPixel, int2(1, 0), centerVelocity, maxVelocityDeltaPixels);
    accumulateVelocityDeltaPixels(centerPixel, int2(0, -1), centerVelocity, maxVelocityDeltaPixels);
    accumulateVelocityDeltaPixels(centerPixel, int2(0, 1), centerVelocity, maxVelocityDeltaPixels);
    return maxVelocityDeltaPixels;
}

void accumulateNeighborhoodSample(int2 centerPixel,
                                  int2 pixelOffset,
                                  inout float3 minColor,
                                  inout float3 maxColor,
                                  inout float3 sumColor,
                                  inout float3 sumColorSquared)
{
    const float3 sampleColor = rgbToYCoCg(loadCurrentColor(centerPixel + pixelOffset).rgb);

    minColor = min(minColor, sampleColor);
    maxColor = max(maxColor, sampleColor);
    sumColor += sampleColor;
    sumColorSquared += sampleColor * sampleColor;
}

float3 clipToAabb(float3 color, float3 aabbMin, float3 aabbMax)
{
    const float3 center = (aabbMin + aabbMax) * 0.5;
    const float3 extents = max((aabbMax - aabbMin) * 0.5, float3(1.0e-5));
    const float3 offset = color - center;
    const float3 unitOffset = abs(offset) / extents;
    const float maxUnit = max(unitOffset.x, max(unitOffset.y, unitOffset.z));

    return maxUnit > 1.0 ? center + offset / maxUnit : color;
}

[shader("vertex")]
TaaVertexOutput taaVertexMain(uint vertexID : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };

    TaaVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    return output;
}

[shader("fragment")]
TaaFragmentOutput taaFragmentMain(TaaVertexOutput input)
{
    const float2 uv = screenUvFromPosition(input.position);
    const int2 centerPixel = pixelFromUv(uv);
    const float4 currentColor = loadCurrentColor(centerPixel);
    const float centerDepth = loadDepth(centerPixel);
    const float2 velocity = loadClosestDepthVelocity(centerPixel, centerDepth);
    const float2 historyUv = uv - velocity;

    float3 minColor = float3(1.0e20);
    float3 maxColor = float3(-1.0e20);
    float3 sumColor = float3(0.0);
    float3 sumColorSquared = float3(0.0);

    accumulateNeighborhoodSample(centerPixel, int2(-1, -1), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(0, -1), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(1, -1), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(-1, 0), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(0, 0), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(1, 0), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(-1, 1), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(0, 1), minColor, maxColor, sumColor, sumColorSquared);
    accumulateNeighborhoodSample(centerPixel, int2(1, 1), minColor, maxColor, sumColor, sumColorSquared);

    const float3 meanColor = sumColor / 9.0;
    const float3 variance = max(sumColorSquared / 9.0 - meanColor * meanColor, float3(0.0));
    const float3 sigma = sqrt(variance);
    const float varianceClipGamma = max(gTaaParams.settings.z, 0.5);
    const float3 varianceMin = meanColor - sigma * varianceClipGamma;
    const float3 varianceMax = meanColor + sigma * varianceClipGamma;
    const float3 clipMin = min(max(minColor, varianceMin), maxColor);
    const float3 clipMax = max(min(maxColor, varianceMax), minColor);

    const bool hasHistory = gTaaParams.settings.y > 0.5 && isInsideScreen(historyUv);
    const float4 rawHistoryColor = hasHistory ? sampleHistoryCatmullRom(historyUv) : currentColor;
    const float3 clippedHistoryYCoCg = clipToAabb(rgbToYCoCg(rawHistoryColor.rgb), clipMin, clipMax);
    const float3 clippedHistoryColor = max(yCoCgToRgb(clippedHistoryYCoCg), float3(0.0));

    const float velocityPixels = length(velocity * gTaaParams.framebuffer.zw);
    const float jitterDeltaPixels = length(gTaaParams.jitterPixels.xy - gTaaParams.jitterPixels.zw);
    const float realVelocityPixels = max(velocityPixels - jitterDeltaPixels, 0.0);
    const float motionReject = saturate(realVelocityPixels * max(gTaaParams.settings.w, 0.0));
    const float currentLum = max(luminance(currentColor.rgb), 1.0e-4);
    const float historyLum = max(luminance(clippedHistoryColor), 1.0e-4);
    const float colorDifference = abs(currentLum - historyLum) / max(max(currentLum, historyLum), 1.0e-4);
    const float colorRejectGate = saturate((realVelocityPixels - 0.50) * 0.50);
    const float colorReject = saturate((colorDifference - 0.20) * 1.25) * colorRejectGate;
    const float velocityDeltaPixels = velocityNeighborhoodDeltaPixels(centerPixel, velocity);
    const float velocityReject = saturate((velocityDeltaPixels - 1.50) * 0.35);
    const float historyWeight =
        hasHistory ? saturate(gTaaParams.settings.x) * (1.0 - motionReject) * (1.0 - colorReject * 0.75) *
                         (1.0 - velocityReject * 0.85)
                   : 0.0;
    const float4 resolvedColor = float4(lerp(currentColor.rgb, clippedHistoryColor, historyWeight), currentColor.a);

    TaaFragmentOutput output;
    output.color = float4(resolvedColor.rgb, currentColor.a);
    output.history = output.color;
    return output;
}

[shader("fragment")]
float4 taaCopyFragmentMain(TaaVertexOutput input) : SV_Target
{
    const float2 uv = screenUvFromPosition(input.position);
    return gCurrentColorTexture.SampleLevel(gTaaSampler, uv, 0.0);
}
