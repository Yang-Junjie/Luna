#version 450

layout(set = 0, binding = 0) uniform GlobalData
{
    vec4 tint;
} uGlobal;

layout(set = 1, binding = 0) uniform MaterialData
{
    vec4 color;
} uMaterial;

layout(set = 1, binding = 1) uniform sampler2D materialTexture;

layout(set = 2, binding = 0) uniform ObjectData
{
    vec4 offsetScale;
} uObject;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

void main()
{
    const vec2 uvNdc = inUV * 2.0 - 1.0;
    const vec2 center = uObject.offsetScale.xy;
    const float scale = max(uObject.offsetScale.z, 0.001);
    const vec2 local = (uvNdc - center) / scale;
    const float radius = length(local);
    const float mask = 1.0 - smoothstep(0.92, 1.0, radius);

    const float texFactor = texture(materialTexture, inUV * 1.5).r;
    const vec3 objectColor = uGlobal.tint.rgb * uMaterial.color.rgb * mix(0.55, 1.0, texFactor);
    const vec3 background = mix(vec3(0.04, 0.05, 0.07), vec3(0.10, 0.11, 0.13), inUV.y);
    const vec3 glow = objectColor * (0.06 / (0.18 + dot(local, local)));
    const vec3 color = mix(background + glow, objectColor, mask);

    outFragColor = vec4(color, 1.0);
}
