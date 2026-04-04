#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

layout(set = 0, binding = 1) uniform MaterialData
{
    vec4 colorFactors;
    vec4 metalRoughFactors;
} materialData;

layout(set = 0, binding = 2) uniform sampler2D colorTex;
layout(set = 0, binding = 3) uniform sampler2D metalRoughTex;

void main()
{
    vec3 normal = normalize(inNormal);
    vec3 lightDirection = normalize(sceneData.sunlightDirection.xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec4 baseColor = texture(colorTex, inUV) * materialData.colorFactors * vec4(inColor, 1.0);
    vec3 lighting = sceneData.ambientColor.xyz + sceneData.sunlightColor.xyz * diffuse;
    outFragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
