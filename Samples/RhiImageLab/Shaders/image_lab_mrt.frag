#version 450

#ifndef MRT_OUTPUT_COUNT
#define MRT_OUTPUT_COUNT 4
#endif

layout(location = 0) in vec2 inUV;

#if MRT_OUTPUT_COUNT >= 1
layout(location = 0) out vec4 outColor0;
#endif
#if MRT_OUTPUT_COUNT >= 2
layout(location = 1) out vec4 outColor1;
#endif
#if MRT_OUTPUT_COUNT >= 3
layout(location = 2) out vec4 outColor2;
#endif
#if MRT_OUTPUT_COUNT >= 4
layout(location = 3) out vec4 outColor3;
#endif

void main()
{
    const vec2 uv = inUV;
    const vec3 normalLike = normalize(vec3(uv * 2.0 - 1.0, 1.0)) * 0.5 + 0.5;
    const float checker = mod(floor(uv.x * 10.0) + floor(uv.y * 10.0), 2.0);

#if MRT_OUTPUT_COUNT >= 1
    outColor0 = vec4(uv, 0.18, 1.0);
#endif
#if MRT_OUTPUT_COUNT >= 2
    outColor1 = vec4(normalLike, 1.0);
#endif
#if MRT_OUTPUT_COUNT >= 3
    outColor2 = vec4(checker, fract(uv.x * 6.0), fract(uv.y * 6.0), 1.0);
#endif
#if MRT_OUTPUT_COUNT >= 4
    outColor3 = vec4(0.25 + 0.75 * uv.x, 0.15 + 0.85 * uv.y, checker, 1.0);
#endif
}
