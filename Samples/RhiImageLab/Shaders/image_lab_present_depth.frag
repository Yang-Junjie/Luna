#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

void main()
{
    const float depth = texture(texture0, inUV).r;
    const float edge = smoothstep(0.15, 0.85, depth);
    outFragColor = vec4(vec3(depth) * vec3(0.85, 0.95, 1.0) + vec3(edge * 0.08), 1.0);
}
