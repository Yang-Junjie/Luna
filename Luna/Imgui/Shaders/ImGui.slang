struct ImGuiVertexInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct ImGuiVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct ImGuiPushConstants
{
    float2 scale;
    float2 translate;
};

#if defined(LUNA_BACKEND_D3D11)
[[vk::push_constant]]
cbuffer PushConstants : register(b13)
{
    ImGuiPushConstants gPushConstants;
}
#elif defined(LUNA_BACKEND_D3D12)
[[vk::push_constant]]
cbuffer PushConstants : register(b15)
{
    ImGuiPushConstants gPushConstants;
}
#else
[[vk::push_constant]]
cbuffer PushConstants
{
    ImGuiPushConstants gPushConstants;
}
#endif

[[vk::binding(0, 0)]]
Texture2D<float4> gTexture : register(t0);

[[vk::binding(1, 0)]]
SamplerState gSampler : register(s1);

[shader("vertex")]
ImGuiVertexOutput imguiVertexMain(ImGuiVertexInput input)
{
    ImGuiVertexOutput output;
    output.uv = input.uv;
    output.color = input.color;

    const float2 clip_position = input.position * gPushConstants.scale + gPushConstants.translate;
    output.position = float4(clip_position, 0.0f, 1.0f);
    return output;
}

[shader("fragment")]
float4 imguiFragmentMain(ImGuiVertexOutput input) : SV_Target
{
    return input.color * gTexture.Sample(gSampler, input.uv);
}
