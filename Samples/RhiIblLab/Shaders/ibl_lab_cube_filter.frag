#version 450

layout(set = 0, binding = 0) uniform samplerCube sourceCube;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform FilterParams
{
    vec4 params;
} PushConstants;

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
    int faceIndex = int(PushConstants.params.x + 0.5);
    float sourceLod = max(PushConstants.params.y, 0.0);
    vec3 direction = face_direction(faceIndex, inUV);
    vec3 color = max(textureLod(sourceCube, direction, sourceLod).rgb, vec3(0.0));
    outFragColor = vec4(color, 1.0);
}
