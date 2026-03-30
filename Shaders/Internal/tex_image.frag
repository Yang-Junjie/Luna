#version 450

layout(set = 1, binding = 0) uniform sampler2D displayTexture;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(displayTexture, inUV) * vec4(inColor, 1.0f);
}
