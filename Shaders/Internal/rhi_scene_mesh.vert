#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;

layout(set = 0, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

layout(push_constant) uniform DrawConstants
{
    mat4 worldMatrix;
} drawData;

void main()
{
    vec4 worldPosition = drawData.worldMatrix * vec4(inPosition, 1.0);
    gl_Position = sceneData.viewproj * worldPosition;
    outNormal = mat3(drawData.worldMatrix) * inNormal;
    outColor = inColor.rgb;
    outUV = inUV;
}
