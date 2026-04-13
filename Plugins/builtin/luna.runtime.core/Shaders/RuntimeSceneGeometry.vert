#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec3 frag_normal;

layout(push_constant) uniform PushConstants {
    mat4 u_model;
    mat4 u_view_projection;
} pc;

void main()
{
    frag_uv = in_uv;
    frag_normal = normalize(mat3(pc.u_model) * in_normal);
    gl_Position = pc.u_view_projection * pc.u_model * vec4(in_position, 1.0);
}
