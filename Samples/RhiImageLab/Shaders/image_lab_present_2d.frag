#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 0, binding = 1) uniform sampler2D texture1;
layout(set = 0, binding = 2) uniform sampler2D texture2;
layout(set = 0, binding = 3) uniform sampler2D texture3;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PresentParams
{
    vec4 params;
} PushConstants;

vec4 sample_texture(int index, vec2 uv, float lod)
{
    switch (index) {
        case 0:
            return textureLod(texture0, uv, lod);
        case 1:
            return textureLod(texture1, uv, lod);
        case 2:
            return textureLod(texture2, uv, lod);
        case 3:
            return textureLod(texture3, uv, lod);
        default:
            return vec4(1.0, 0.0, 1.0, 1.0);
    }
}

void main()
{
    const int mode = int(PushConstants.params.x + 0.5);
    const int previewIndex = int(PushConstants.params.y + 0.5);
    const float lod = max(PushConstants.params.z, 0.0);
    const int activeTextureCount = max(int(PushConstants.params.w + 0.5), 1);

    if (mode == 1) {
        const vec2 tiledUV = fract(inUV * 2.0);
        const ivec2 tile = ivec2(clamp(floor(inUV * 2.0), vec2(0.0), vec2(1.0)));
        const int tileIndex = clamp(tile.x + tile.y * 2, 0, activeTextureCount - 1);

        vec4 color = sample_texture(tileIndex, tiledUV, 0.0);
        const vec2 border = abs(fract(inUV * 2.0) - 0.5);
        if (border.x > 0.492 || border.y > 0.492) {
            color.rgb = mix(color.rgb, vec3(0.05, 0.05, 0.05), 0.65);
        }
        outFragColor = color;
        return;
    }

    outFragColor = sample_texture(clamp(previewIndex, 0, activeTextureCount - 1), inUV, lod);
}
