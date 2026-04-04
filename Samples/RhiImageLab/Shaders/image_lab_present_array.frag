#version 450

layout(set = 0, binding = 0) uniform sampler2DArray sourceTexture;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform ArrayParams
{
    vec4 params;
} PushConstants;

void main()
{
    const float layer = max(PushConstants.params.x, 0.0);
    outFragColor = texture(sourceTexture, vec3(inUV, layer));
}
