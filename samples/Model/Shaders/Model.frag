#version 450

layout(set = 0, binding = 0) uniform sampler2D uAlbedoTexture;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 albedo = texture(uAlbedoTexture, fragUv).rgb;
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(0.45, 0.8, 0.35));
    float diffuse = max(dot(normal, lightDirection), 0.15);
    outColor = vec4(albedo * diffuse, 1.0);
}
