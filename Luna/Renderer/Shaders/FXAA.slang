struct FXAAVertexOutput
{
    float4 position : SV_Position;
};

struct FXAAParams
{
    float4 framebuffer;
    float4 settings;
};

[[vk::binding(0, 0)]]
Texture2D<float4> gSourceColorTexture : register(t0);

[[vk::binding(1, 0)]]
SamplerState gSourceColorSampler : register(s1);

[[vk::binding(2, 0)]]
ConstantBuffer<FXAAParams> gFXAAParams : register(b2);

float2 screenUvFromPosition(float4 position)
{
    return saturate(position.xy * gFXAAParams.framebuffer.xy);
}

float luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

float3 sampleSource(float2 uv)
{
    return gSourceColorTexture.SampleLevel(gSourceColorSampler, saturate(uv), 0.0).rgb;
}

float4 loadSource(int2 pixel)
{
    const int2 framebufferSize = int2(int(gFXAAParams.framebuffer.z), int(gFXAAParams.framebuffer.w));
    const int2 clampedPixel = clamp(pixel, int2(0, 0), framebufferSize - int2(1, 1));
    return gSourceColorTexture.Load(int3(clampedPixel.x, clampedPixel.y, 0));
}

int2 pixelFromUv(float2 uv)
{
    const int2 framebufferSize = int2(int(gFXAAParams.framebuffer.z), int(gFXAAParams.framebuffer.w));
    return clamp(int2(uv * gFXAAParams.framebuffer.zw), int2(0, 0), framebufferSize - int2(1, 1));
}

[shader("vertex")]
FXAAVertexOutput FXAAVertexMain(uint vertexID : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };

    FXAAVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 FXAAFragmentMain(FXAAVertexOutput input) : SV_Target
{
    const float2 uv = screenUvFromPosition(input.position);
    const int2 centerPixel = pixelFromUv(uv);
    const float2 texel = gFXAAParams.framebuffer.xy;

    const float3 colorCenter = loadSource(centerPixel).rgb;
    const float lumaCenter = luminance(colorCenter);
    const float lumaNorth = luminance(loadSource(centerPixel + int2(0, -1)).rgb);
    const float lumaSouth = luminance(loadSource(centerPixel + int2(0, 1)).rgb);
    const float lumaWest = luminance(loadSource(centerPixel + int2(-1, 0)).rgb);
    const float lumaEast = luminance(loadSource(centerPixel + int2(1, 0)).rgb);

    const float lumaMin = min(lumaCenter, min(min(lumaNorth, lumaSouth), min(lumaWest, lumaEast)));
    const float lumaMax = max(lumaCenter, max(max(lumaNorth, lumaSouth), max(lumaWest, lumaEast)));
    const float lumaRange = lumaMax - lumaMin;
    const float edgeThreshold = max(gFXAAParams.settings.y, 0.0312);
    const float edgeThresholdMin = max(gFXAAParams.settings.z, 0.0);
    if (lumaRange < max(edgeThresholdMin, lumaMax * edgeThreshold))
    {
        return float4(colorCenter, 1.0);
    }

    const float lumaNorthWest = luminance(loadSource(centerPixel + int2(-1, -1)).rgb);
    const float lumaNorthEast = luminance(loadSource(centerPixel + int2(1, -1)).rgb);
    const float lumaSouthWest = luminance(loadSource(centerPixel + int2(-1, 1)).rgb);
    const float lumaSouthEast = luminance(loadSource(centerPixel + int2(1, 1)).rgb);

    const float edgeHorizontal =
        abs(-2.0 * lumaWest + lumaNorthWest + lumaSouthWest) +
        abs(-2.0 * lumaCenter + lumaNorth + lumaSouth) * 2.0 +
        abs(-2.0 * lumaEast + lumaNorthEast + lumaSouthEast);
    const float edgeVertical =
        abs(-2.0 * lumaNorth + lumaNorthWest + lumaNorthEast) +
        abs(-2.0 * lumaCenter + lumaWest + lumaEast) * 2.0 +
        abs(-2.0 * lumaSouth + lumaSouthWest + lumaSouthEast);
    const bool horizontal = edgeHorizontal >= edgeVertical;

    const float lumaNegative = horizontal ? lumaNorth : lumaWest;
    const float lumaPositive = horizontal ? lumaSouth : lumaEast;
    const float gradientNegative = abs(lumaNegative - lumaCenter);
    const float gradientPositive = abs(lumaPositive - lumaCenter);
    const bool pairNegative = gradientNegative >= gradientPositive;
    const float gradient = max(gradientNegative, gradientPositive);
    const float lumaLocalAverage = 0.5 * (pairNegative ? lumaNegative + lumaCenter : lumaPositive + lumaCenter);

    const float2 edgeStep = horizontal ? float2(0.0, texel.y) : float2(texel.x, 0.0);
    const float2 searchStep = horizontal ? float2(texel.x, 0.0) : float2(0.0, texel.y);
    float2 edgeUv = uv + edgeStep * (pairNegative ? -0.5 : 0.5);

    float2 uvNegative = edgeUv - searchStep;
    float2 uvPositive = edgeUv + searchStep;
    const float gradientScaled = gradient * 0.25;
    bool doneNegative = false;
    bool donePositive = false;
    float lumaEndNegative = 0.0;
    float lumaEndPositive = 0.0;

    [unroll]
    for (int i = 0; i < 12; ++i)
    {
        if (!doneNegative)
        {
            lumaEndNegative = luminance(sampleSource(uvNegative)) - lumaLocalAverage;
            doneNegative = abs(lumaEndNegative) >= gradientScaled;
            uvNegative -= searchStep;
        }
        if (!donePositive)
        {
            lumaEndPositive = luminance(sampleSource(uvPositive)) - lumaLocalAverage;
            donePositive = abs(lumaEndPositive) >= gradientScaled;
            uvPositive += searchStep;
        }
        if (doneNegative && donePositive)
        {
            break;
        }
    }

    const float distanceNegative = horizontal ? uv.x - uvNegative.x : uv.y - uvNegative.y;
    const float distancePositive = horizontal ? uvPositive.x - uv.x : uvPositive.y - uv.y;
    const bool directionNegative = distanceNegative < distancePositive;
    const float distanceFinal = min(distanceNegative, distancePositive);
    const float edgeThickness = max(distanceNegative + distancePositive, 1.0e-5);
    const float pixelOffset = -distanceFinal / edgeThickness + 0.5;
    const bool lumaCenterBelow = lumaCenter < lumaLocalAverage;
    const bool correctVariation =
        ((directionNegative ? lumaEndNegative : lumaEndPositive) < 0.0) != lumaCenterBelow;
    const float edgeOffset = correctVariation ? pixelOffset : 0.0;

    const float lumaAverage =
        (lumaNorthWest + lumaNorth + lumaNorthEast + lumaWest + lumaEast + lumaSouthWest + lumaSouth + lumaSouthEast) *
        (1.0 / 8.0);
    const float subpixelQuality = saturate(gFXAAParams.settings.x);
    const float subpixelOffset =
        pow(saturate(abs(lumaAverage - lumaCenter) / max(lumaRange, 1.0e-5)), 2.0) * subpixelQuality;
    const float finalOffset = max(edgeOffset, subpixelOffset);
    const float2 finalUv = horizontal ? uv + float2(0.0, finalOffset * (pairNegative ? -texel.y : texel.y))
                                      : uv + float2(finalOffset * (pairNegative ? -texel.x : texel.x), 0.0);

    return float4(sampleSource(finalUv), 1.0);
}

[shader("fragment")]
float4 FXAACopyFragmentMain(FXAAVertexOutput input) : SV_Target
{
    return float4(sampleSource(screenUvFromPosition(input.position)), 1.0);
}
