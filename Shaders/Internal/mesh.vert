#version 460
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(set = 0, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

layout(push_constant) uniform constants
{
    mat4 render_matrix;
    VertexBuffer vertexBuffer;
} PushConstants;

void main()
{
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = PushConstants.render_matrix * vec4(v.position, 1.0f);

    gl_Position = sceneData.viewproj * worldPos;
    outNormal = mat3(PushConstants.render_matrix) * v.normal;
    outColor = v.color.xyz;
    outUV = vec2(v.uv_x, v.uv_y);
}
