#version 450

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform DrawParams
{
    vec4 params;
} PushConstants;

void main()
{
    const vec2 basePositions[3] = vec2[3](
        vec2(-0.46, -0.32),
        vec2(0.40, -0.36),
        vec2(-0.10, 0.44)
    );

    const vec2 position = basePositions[gl_VertexIndex] * PushConstants.params.z + PushConstants.params.xy;
    gl_Position = vec4(position, 0.0, 1.0);
    outUV = basePositions[gl_VertexIndex] * 0.5 + 0.5;
}
