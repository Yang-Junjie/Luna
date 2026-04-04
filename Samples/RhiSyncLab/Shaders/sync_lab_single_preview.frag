#version 450

layout(set = 0, binding = 0) uniform sampler2D previewImage;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

void main()
{
    vec4 color = texture(previewImage, inUV);
    const vec2 edge = abs(inUV - 0.5);
    const float vignette = 1.0 - smoothstep(0.30, 0.72, max(edge.x, edge.y));
    color.rgb *= mix(0.80, 1.08, vignette);
    outFragColor = color;
}
