#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 fragUv;

layout(push_constant) uniform PushConstants {
    mat4 uModel;
    mat4 uViewProjection;
} pc;

void main()
{
    fragUv = inUv;
    gl_Position = pc.uViewProjection * pc.uModel * vec4(inPosition, 1.0);
}
