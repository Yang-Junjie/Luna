#version 450

layout(set = 0, binding = 0) uniform DynamicObjectData
{
    vec4 color;
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
    const float disc = 1.0 - smoothstep(0.88, 1.0, radius);
    const float ring = smoothstep(0.78, 0.90, radius) - smoothstep(0.90, 1.02, radius);

    const vec3 background = mix(vec3(0.02, 0.03, 0.05), vec3(0.08, 0.09, 0.14), inUV.y);
    vec3 color = background;
    color += uObject.color.rgb * disc;
    color += uObject.color.rgb * ring * 0.6;

    outFragColor = vec4(color, 1.0);
}
