#version 450

layout(set = 0, binding = 0) uniform sampler2D textures[4];

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform ArrayParams
{
    ivec4 params;
} PushConstants;

void main()
{
    const int index = clamp(PushConstants.params.x, 0, 3);
    vec4 color = texture(textures[index], inUV);

    const vec2 edge = abs(inUV - 0.5);
    const float vignette = 1.0 - smoothstep(0.30, 0.72, max(edge.x, edge.y));
    color.rgb *= mix(0.78, 1.08, vignette);

    outFragColor = color;
}
