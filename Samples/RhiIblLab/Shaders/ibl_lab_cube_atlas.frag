#version 450

layout(set = 0, binding = 0) uniform samplerCube envCube;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform AtlasParams
{
    vec4 params;
} PushConstants;

vec3 tonemap(vec3 color)
{
    vec3 mapped = color / (vec3(1.0) + max(color, vec3(0.0)));
    return pow(clamp(mapped, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
}

vec3 face_direction(int faceIndex, vec2 uv)
{
    vec2 p = uv * 2.0 - 1.0;
    p.y = -p.y;

    switch (faceIndex) {
        case 0:
            return normalize(vec3(1.0, p.y, -p.x));
        case 1:
            return normalize(vec3(-1.0, p.y, p.x));
        case 2:
            return normalize(vec3(p.x, 1.0, -p.y));
        case 3:
            return normalize(vec3(p.x, -1.0, p.y));
        case 4:
            return normalize(vec3(p.x, p.y, 1.0));
        default:
            return normalize(vec3(-p.x, p.y, -1.0));
    }
}

void main()
{
    vec2 gridUV = inUV * vec2(3.0, 2.0);
    ivec2 tile = ivec2(floor(gridUV));
    vec2 localUV = fract(gridUV);
    int faceIndex = tile.x + tile.y * 3;

    if (tile.x < 0 || tile.x >= 3 || tile.y < 0 || tile.y >= 2) {
        outFragColor = vec4(0.03, 0.04, 0.05, 1.0);
        return;
    }

    vec3 color = textureLod(envCube, face_direction(faceIndex, localUV), max(PushConstants.params.y, 0.0)).rgb;
    color = tonemap(color);

    float border = step(localUV.x, 0.025) + step(localUV.y, 0.025) + step(0.975, localUV.x) + step(0.975, localUV.y);
    if (border > 0.0) {
        color *= 0.35;
    }

    if (faceIndex == int(PushConstants.params.x + 0.5)) {
        float highlight = step(localUV.x, 0.05) + step(localUV.y, 0.05) + step(0.95, localUV.x) + step(0.95, localUV.y);
        color = mix(color, vec3(1.0), min(highlight, 1.0) * 0.9);
    }

    outFragColor = vec4(color, 1.0);
}
