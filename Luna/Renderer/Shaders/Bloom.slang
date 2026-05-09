struct BloomVertexOutput
{
    float4 position : SV_Position;
};

struct BloomParams
{
    float4 targetSize;
    float4 filterParams;
};

[[vk::binding(0, 0)]]
Texture2D<float4> gBloomSourceTexture : register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> gBloomTexture : register(t1);

[[vk::binding(2, 0)]]
SamplerState gBloomSampler : register(s2);

[[vk::binding(3, 0)]]
ConstantBuffer<BloomParams> gBloomParams : register(b3);

float2 screenUvFromPosition(float4 position)
{
    return saturate(position.xy * gBloomParams.targetSize.zw);
}

float2 sourceTexelSize()
{
    uint width = 1;
    uint height = 1;
    gBloomSourceTexture.GetDimensions(width, height);
    return 1.0 / max(float2(width, height), float2(1.0, 1.0));
}

float3 sampleSource(float2 uv)
{
    return max(gBloomSourceTexture.SampleLevel(gBloomSampler, saturate(uv), 0.0).rgb, float3(0.0));
}

float3 sampleBloom(float2 uv)
{
    return max(gBloomTexture.SampleLevel(gBloomSampler, saturate(uv), 0.0).rgb, float3(0.0));
}

float3 filterBloomThreshold(float3 color)
{
    const float threshold = max(gBloomParams.filterParams.x, 0.0);
    const float softKnee = saturate(gBloomParams.filterParams.y);
    const float brightness = max(color.r, max(color.g, color.b));
    const float knee = max(threshold * softKnee, 1.0e-5);
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    const float contribution = max(soft, brightness - threshold) / max(brightness, 1.0e-5);
    return color * saturate(contribution);
}

float3 downsampleSource(float2 uv)
{
    const float2 texel = sourceTexelSize();
    float3 color = sampleSource(uv) * 0.25;
    color += sampleSource(uv + texel * float2(1.0, 0.0)) * 0.125;
    color += sampleSource(uv + texel * float2(-1.0, 0.0)) * 0.125;
    color += sampleSource(uv + texel * float2(0.0, 1.0)) * 0.125;
    color += sampleSource(uv + texel * float2(0.0, -1.0)) * 0.125;
    color += sampleSource(uv + texel * float2(1.0, 1.0)) * 0.0625;
    color += sampleSource(uv + texel * float2(-1.0, 1.0)) * 0.0625;
    color += sampleSource(uv + texel * float2(1.0, -1.0)) * 0.0625;
    color += sampleSource(uv + texel * float2(-1.0, -1.0)) * 0.0625;
    return color;
}

float3 upsampleSource(float2 uv)
{
    const float radius = max(gBloomParams.filterParams.w, 0.01);
    const float2 texel = sourceTexelSize() * radius;
    float3 color = sampleSource(uv) * 4.0;
    color += sampleSource(uv + texel * float2(1.0, 0.0)) * 2.0;
    color += sampleSource(uv + texel * float2(-1.0, 0.0)) * 2.0;
    color += sampleSource(uv + texel * float2(0.0, 1.0)) * 2.0;
    color += sampleSource(uv + texel * float2(0.0, -1.0)) * 2.0;
    color += sampleSource(uv + texel * float2(1.0, 1.0));
    color += sampleSource(uv + texel * float2(-1.0, 1.0));
    color += sampleSource(uv + texel * float2(1.0, -1.0));
    color += sampleSource(uv + texel * float2(-1.0, -1.0));
    return color * 0.0625;
}

[shader("vertex")]
BloomVertexOutput bloomVertexMain(uint vertexID : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };

    BloomVertexOutput output;
    output.position = float4(positions[vertexID], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 bloomPrefilterFragmentMain(BloomVertexOutput input) : SV_Target
{
    const float2 uv = screenUvFromPosition(input.position);
    return float4(filterBloomThreshold(downsampleSource(uv)), 1.0);
}

[shader("fragment")]
float4 bloomDownsampleFragmentMain(BloomVertexOutput input) : SV_Target
{
    const float2 uv = screenUvFromPosition(input.position);
    return float4(downsampleSource(uv), 1.0);
}

[shader("fragment")]
float4 bloomUpsampleFragmentMain(BloomVertexOutput input) : SV_Target
{
    const float2 uv = screenUvFromPosition(input.position);
    return float4(upsampleSource(uv), 1.0);
}

[shader("fragment")]
float4 bloomCompositeFragmentMain(BloomVertexOutput input) : SV_Target
{
    const float2 uv = screenUvFromPosition(input.position);
    const float intensity = max(gBloomParams.filterParams.z, 0.0);
    return float4(sampleSource(uv) + sampleBloom(uv) * intensity, 1.0);
}
