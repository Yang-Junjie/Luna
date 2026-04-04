#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

void main()
{
    const vec3 base = mix(vec3(0.08, 0.12, 0.18), vec3(0.92, 0.42, 0.14), inUV.x);
    const float band = 0.5 + 0.5 * sin((inUV.x + inUV.y) * 22.0);
    outFragColor = vec4(base + band * vec3(0.03, 0.02, 0.01), 1.0);
}
