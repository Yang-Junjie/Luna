#version 450

layout(set = 0, binding = 0) uniform samplerCube envCube;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform SkyboxParams
{
    vec4 params;
} PushConstants;

vec3 tonemap(vec3 color)
{
    vec3 mapped = color / (vec3(1.0) + max(color, vec3(0.0)));
    return pow(clamp(mapped, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
}

vec3 rotate_yaw_pitch(vec3 dir, float yaw, float pitch)
{
    float cy = cos(yaw);
    float sy = sin(yaw);
    vec3 yawed = vec3(cy * dir.x + sy * dir.z, dir.y, -sy * dir.x + cy * dir.z);

    float cp = cos(pitch);
    float sp = sin(pitch);
    return vec3(yawed.x, cp * yawed.y - sp * yawed.z, sp * yawed.y + cp * yawed.z);
}

void main()
{
    vec2 ndc = inUV * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float tanHalfFov = tan(radians(45.0));
    vec3 dir = normalize(vec3(ndc.x * PushConstants.params.z * tanHalfFov, ndc.y * tanHalfFov, 1.0));
    dir = rotate_yaw_pitch(dir, PushConstants.params.x, PushConstants.params.y);

    vec3 color = tonemap(texture(envCube, dir).rgb);
    outFragColor = vec4(color, 1.0);
}
