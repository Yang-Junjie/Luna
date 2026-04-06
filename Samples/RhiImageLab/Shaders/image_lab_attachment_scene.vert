#version 450

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform SceneParams
{
    vec4 params;
} PushConstants;

void main()
{
    const vec2 basePositions[3] = vec2[3](
        vec2(-0.40, -0.32),
        vec2(0.42, -0.34),
        vec2(-0.08, 0.46)
    );
    const float depths[3] = float[3](0.18, 0.86, 0.48);

    const vec2 position = basePositions[gl_VertexIndex] * PushConstants.params.z + PushConstants.params.xy;
    gl_Position = vec4(position, depths[gl_VertexIndex], 1.0);
    outUV = basePositions[gl_VertexIndex] * 0.5 + 0.5;
}
