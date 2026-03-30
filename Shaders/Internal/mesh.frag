#version 460

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

layout(set = 1, binding = 0) uniform MaterialData
{
    vec4 colorFactors;
    vec4 metal_rough_factors;
    vec4 extra[14];
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;

void main()
{
    vec3 N = normalize(inNormal);
    vec3 L = normalize(sceneData.sunlightDirection.xyz);
    float diffuse = max(dot(N, L), 0.0f);

    vec4 baseColor = texture(colorTex, inUV) * materialData.colorFactors * vec4(inColor, 1.0f);
    vec3 lighting = sceneData.ambientColor.xyz + sceneData.sunlightColor.xyz * diffuse;

    outFragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
