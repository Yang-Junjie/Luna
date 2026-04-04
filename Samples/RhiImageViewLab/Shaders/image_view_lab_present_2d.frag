#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PresentParams
{
    vec4 params;
} PushConstants;

void main()
{
    const float lod = max(PushConstants.params.x, 0.0);
    outFragColor = textureLod(sourceTexture, inUV, lod);
}
