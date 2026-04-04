#version 450

layout(set = 0, binding = 0) uniform sampler2D currentImage;
layout(set = 0, binding = 1) uniform sampler2D historyImage;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

void main()
{
    const bool showCurrent = inUV.x < 0.5;
    const vec2 uv = vec2(showCurrent ? inUV.x * 2.0 : (inUV.x - 0.5) * 2.0, inUV.y);
    vec4 color = showCurrent ? texture(currentImage, uv) : texture(historyImage, uv);

    const float divider = smoothstep(0.498, 0.500, inUV.x) - smoothstep(0.500, 0.502, inUV.x);
    color.rgb = mix(color.rgb, vec3(1.0), clamp(divider * 20.0, 0.0, 1.0));
    outFragColor = color;
}
