#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 outColor;

layout(push_constant) uniform QuadPushConstants {
    vec4 tint;
    vec2 offset;
    float scale;
    float mixFactor;
} pc;

void main()
{
    gl_Position = vec4(inPosition * pc.scale + pc.offset, 0.0, 1.0);
    outColor = inColor;
}
