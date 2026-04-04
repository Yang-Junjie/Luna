#version 450

layout(set = 0, binding = 0) uniform sampler3D sourceTexture;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform VolumeParams
{
    vec4 params;
} PushConstants;

void main()
{
    const float slice = clamp(PushConstants.params.x, 0.0, 1.0);
    outFragColor = texture(sourceTexture, vec3(inUV, slice));
}
