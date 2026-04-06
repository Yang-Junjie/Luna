#version 450

layout(set = 0, binding = 0) uniform sampler2D faceTexture;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform FacePreviewParams
{
    vec4 params;
} PushConstants;

vec3 tonemap(vec3 color)
{
    vec3 mapped = color / (vec3(1.0) + max(color, vec3(0.0)));
    return pow(clamp(mapped, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
}

void main()
{
    float lod = max(PushConstants.params.x, 0.0);
    vec4 color = textureLod(faceTexture, inUV, lod);
    if (PushConstants.params.y > 0.5) {
        color.rgb = tonemap(color.rgb);
    }
    outFragColor = color;
}
