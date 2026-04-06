#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform SceneParams
{
    vec4 params;
} PushConstants;

void main()
{
    const float pulse = 0.5 + 0.5 * sin(PushConstants.params.w * 6.2831853);
    const float checker = mod(floor(inUV.x * 8.0) + floor(inUV.y * 8.0), 2.0);
    const vec3 base = vec3(0.18 + 0.72 * inUV.x, 0.24 + 0.60 * inUV.y, 0.18 + 0.72 * pulse);
    outFragColor = vec4(mix(base, base.bgr, checker * 0.18), 1.0);
}
