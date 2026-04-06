#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform DrawParams
{
    vec4 params;
} PushConstants;

void main()
{
    const float pulse = 0.5 + 0.5 * sin(PushConstants.params.w * 6.2831853);
    const vec3 color = vec3(0.20 + 0.70 * inUV.x, 0.18 + 0.72 * pulse, 0.25 + 0.65 * inUV.y);
    outFragColor = vec4(color, 1.0);
}
