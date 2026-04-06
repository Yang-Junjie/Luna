#version 450

layout(set = 0, binding = 0, std430) readonly buffer IndirectArgs
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
    uint gridCountX;
    uint gridCountY;
} argsBuffer;

layout(location = 0) out vec3 outColor;

void main()
{
    const vec2 triangle[3] = vec2[3](
        vec2(-0.42, -0.36),
        vec2(0.44, -0.34),
        vec2(-0.08, 0.46)
    );

    const uint gridX = max(argsBuffer.gridCountX, 1u);
    const uint gridY = max(argsBuffer.gridCountY, 1u);
    const uint instance = uint(gl_InstanceIndex);
    const uint cellX = instance % gridX;
    const uint cellY = instance / gridX;

    const vec2 cellSize = vec2(2.0 / float(gridX), 2.0 / float(gridY));
    const vec2 center = vec2(-1.0 + cellSize.x * (float(cellX) + 0.5),
                             -1.0 + cellSize.y * (float(cellY) + 0.5));
    const vec2 position = center + triangle[gl_VertexIndex] * cellSize * 0.42;
    gl_Position = vec4(position, 0.0, 1.0);

    outColor = vec3((float(cellX) + 1.0) / float(gridX),
                    (float(cellY) + 1.0) / float(gridY),
                    0.35 + 0.55 * fract(float(instance) * 0.173));
}
