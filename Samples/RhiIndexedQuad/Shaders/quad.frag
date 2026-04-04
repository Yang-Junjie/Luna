#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform QuadPushConstants {
    vec4 tint;
    vec2 offset;
    float scale;
    float mixFactor;
} pc;

void main()
{
    const float mixFactor = clamp(pc.mixFactor, 0.0, 1.0);
    outFragColor = vec4(mix(inColor, pc.tint.rgb, mixFactor), 1.0);
}
